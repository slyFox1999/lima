// Copyright 2021 CEA LIST
// SPDX-FileCopyrightText: 2022 CEA LIST <gael.de-chalendar@cea.fr>
//
// SPDX-License-Identifier: MIT

#ifndef DEEPLIMA_GRAPH_DP_IMPL_H
#define DEEPLIMA_GRAPH_DP_IMPL_H

#include "deeplima/token_type.h"
#include "deeplima/fastText_wrp/fastText_wrp.h"
#include "deeplima/eigen_wrp/embd_dict.h"
#include "deeplima/utils/str_index.h"
#include "deeplima/utils/std_matrix.h"

#include "helpers/path_resolver.h"

namespace deeplima
{
namespace graph_dp
{

namespace impl {

template <class InferenceEngine, class Vectorizer, class Matrix>
class GraphDpImpl: public InferenceEngine
{
public:

  GraphDpImpl() :
      m_fastText(std::make_shared<FastTextVectorizer<typename Matrix::matrix_t, Eigen::Index>>()),
      m_current_slot_timepoints(0),
      m_current_slot_no(-1),
      m_last_completed_slot(-1),
      m_curr_buff_idx(0)
  {}

  GraphDpImpl(
      size_t threads,
      size_t buffer_size_per_thread
    )
    : InferenceEngine(
        0 /* TODO: FIX ME */, 4, threads * 2, buffer_size_per_thread, threads),
      m_fastText(std::make_shared<FastTextVectorizer<typename Matrix::matrix_t, Eigen::Index>>()),
      m_current_timepoint(InferenceEngine::get_start_timepoint())
  {
  }

  std::shared_ptr<EmbdUInt64Float> convert(const EmbdStrFloat& src)
  {
    auto d = src.get_int_dict<EmbdUInt64Float::value_t>();
    auto t = src.get_tensor().transpose();
    auto p = std::make_shared<EmbdUInt64Float>();
    p->init(d, t);

    return p;
  }

  virtual void load(const std::string& fn, const PathResolver& path_resolver)
  {
    InferenceEngine::load(fn);

    m_featVectorizers.resize(this->get_input_str_dicts().size());
    for (size_t i = 0; i < this->get_input_str_dicts().size(); ++i)
    {
      //auto d = (this->get_str_dicts()[i]);
      m_featVectorizers[i] = convert(this->get_input_str_dicts()[i]);
    }

    m_fastText->load(path_resolver.resolve("embd", InferenceEngine::get_embd_fn(0), {"bin", "ftz"}));
  }

  void init(size_t threads,
            size_t num_buffers,
            size_t buffer_size_per_thread,
            StringIndex& stridx,
            const std::vector<std::string>& class_names)
  {
    m_fastText->get_words([&stridx](const std::string& word){ stridx.get_idx(word); });

    std::vector<typename Vectorizer::feature_descr_t> feats;
    feats.reserve(1/* + m_featVectorizers.size()*/);
    feats.emplace_back(Vectorizer::str_feature, "form", m_fastText);
    for (size_t i = 0; i < class_names.size(); ++i)
    {
      if (class_names[i] != InferenceEngine::get_input_str_dicts_names()[i+1])
      {
        // TODO: skip morph classes that aren't requested by DP
        throw std::logic_error("Input classes missmatch: " + class_names[i] + " != " + InferenceEngine::get_input_str_dicts_names()[i+1]);
      }

      feats.emplace_back(Vectorizer::int_feature,
                         InferenceEngine::get_input_str_dicts_names()[i+1],
                         m_featVectorizers[i]);

      m_vectorizer.get_uint_feat_extractor().add_feature(class_names[i], i);
    }
    // for (const auto& class_name: class_names)
    // {
    //   int i = 0;
    //   for (const auto& input_str_dicts_names: InferenceEngine::get_input_str_dicts_names())
    //   {
    //     if (class_name == input_str_dicts_names)
    //     {
    //       feats.emplace_back(Vectorizer::int_feature,
    //                         class_name,
    //                         m_featVectorizers[i]);
    //
    //       m_vectorizer.get_uint_feat_extractor().add_feature(class_name, i);
    //       break;
    //     }
    //     i++;
    //   }
    // }
    m_vectorizer.init_features(feats);

    m_vectorizer.set_model(this);

    InferenceEngine::init(m_vectorizer.dim(),
                          0, num_buffers, buffer_size_per_thread, threads,
                          m_vectorizer.is_precomputing());

    m_current_timepoint = InferenceEngine::get_start_timepoint();
  }

  void precompute_inputs(const typename Vectorizer::dataset_t& buffer)
  {
    m_vectorizer.precompute(buffer);
  }

  typedef std::function < void (std::shared_ptr< StdMatrix<uint32_t> > classes,
                                size_t begin, size_t end, size_t slot_idx) > tagging_callback_t;

  virtual void register_handler(const tagging_callback_t fn)
  {
    m_callback = fn;
  }

  virtual ~GraphDpImpl()
  {
    // std::cerr << "~GraphDpImpl" << std::endl;
  }

protected:

  inline void increment_timepoint(uint64_t& timepoint)
  {
    assert(m_current_slot_timepoints > 0);
    InferenceEngine::increment_timepoint(timepoint);
    m_current_slot_timepoints--;
  }

  inline void send_results(int32_t slot_idx)
  {
    uint8_t lock_count = InferenceEngine::get_lock_count(slot_idx);
    // std::cerr << "GraphDpImpl::send_results " << slot_idx+1
    //       << " (lock_count=" << int(lock_count) << ")\n";
    uint64_t from = InferenceEngine::get_slot_begin(slot_idx);
    const uint64_t to = InferenceEngine::get_slot_end(slot_idx);
    // std::cerr << "GraphDpImpl::send_results " << slot_idx+1 << ", from=" << from << ", to=" << to << std::endl;

    m_callback(InferenceEngine::get_output(), from, to, slot_idx);

    InferenceEngine::decrement_lock_count(slot_idx);
    m_last_completed_slot = slot_idx;
  }

public:
  inline void send_next_results()
  {
    auto slot_idx = m_last_completed_slot;
    // std::cerr << "GraphDpImpl::send_next_results " << slot_idx+1 << std::endl;
    if (-1 == slot_idx)
    {
      slot_idx = 0;
    }
    else
    {
      slot_idx = InferenceEngine::next_slot(slot_idx);
    }
    // int a=0;
    uint8_t lock_count = InferenceEngine::get_lock_count(slot_idx);
    // std::cerr << "GraphDpImpl::send_next_results " << slot_idx+1
    //           << " (lock_count=" << int(lock_count) << ")" << std::endl;

    while (lock_count > 1)
    {
      // Worker still uses this slot. Waiting...
      // std::cerr << "GraphDpImpl::send_next_results: waiting for slot " << slot_idx+1
      //      << " (lock_count==" << int(lock_count) << ")\n";
      // InferenceEngine::pretty_print();
      InferenceEngine::wait_for_slot(slot_idx);
      lock_count = InferenceEngine::get_lock_count(slot_idx);
    }
    if (1 == lock_count)
    {
      // Data is ready. We can return it to caller
      send_results(slot_idx);
    }
  }

  inline void send_all_results()
  {
    auto slot_idx = m_last_completed_slot;
    // std::cerr << "GraphDpImpl::send_all_results" << slot_idx << std::endl;
    while (true)
    {
      if (-1 == slot_idx)
      {
        slot_idx = 0;
      }
      else
      {
        slot_idx = InferenceEngine::next_slot(slot_idx);
      }

      uint8_t lock_count = InferenceEngine::get_lock_count(slot_idx);
      if (0 == lock_count)
      {
        // std::cerr << "GraphDpImpl::send_all_results DONE" << std::endl;
        return;
      }

      while (lock_count > 1)
      {
        // Worker still uses this slot. Waiting...
        // std::cerr << "GraphDpImpl::send_all_results: waiting for slot " << slot_idx+1
        //      << " (lock_count==" << int(lock_count) << ")\n";
        // InferenceEngine::pretty_print();
        InferenceEngine::wait_for_slot(slot_idx);
        lock_count = InferenceEngine::get_lock_count(slot_idx);
      }
      if (1 == lock_count)
      {
        send_results(slot_idx);
      }
    }
  }

protected:
  inline void send_results_if_available()
  {
    auto slot_idx = m_last_completed_slot;
    if (-1 == slot_idx)
    {
      slot_idx = 0;
    }
    else
    {
      slot_idx = InferenceEngine::next_slot(slot_idx);
    }

    uint8_t lock_count = InferenceEngine::get_lock_count(slot_idx);

    if (1 == lock_count)
    {
      // Data is ready. We can return it to caller
      send_results(slot_idx);
    }
  }

  inline void acquire_slot(size_t slot_no)
  {
    // m_current_slot_no = InferenceEngine::get_slot_idx(m_current_timepoint);
    uint8_t lock_count = InferenceEngine::get_lock_count(slot_no);
    // std::cerr << "GraphDpImpl::acquiring_slot: " << (slot_no+1) << "; current lock count: " << int(lock_count) << std::endl;
    // InferenceEngine::pretty_print();

    while (lock_count > 1)
    {
      // Worker still uses this slot. Waiting...
      // std::cerr << "GraphDpImpl::acquiring_slot, waiting for slot " << (slot_no+1)
      //      << " lock_count=" << int(lock_count) << std::endl;
      // InferenceEngine::pretty_print();
      InferenceEngine::wait_for_slot(slot_no);
      lock_count = InferenceEngine::get_lock_count(slot_no);
    }
    if (1 == lock_count)
    {
      // Data is ready. We can return it to caller
      send_results(slot_no);
    }

    lock_count = InferenceEngine::get_lock_count(slot_no);
    // std::cerr << "GraphDpImpl::acquiring_slot: " << (slot_no+1) << "; before increment_lock_count: " << int(lock_count) << std::endl;
    // InferenceEngine::pretty_print();
    InferenceEngine::increment_lock_count(slot_no);
    lock_count = InferenceEngine::get_lock_count(slot_no);
    // std::cerr << "GraphDpImpl::acquiring_slot: " << (slot_no+1) << "; after increment_lock_count: " << int(lock_count) << std::endl;
    // InferenceEngine::pretty_print();
  }

public:
  virtual void handle_token_buffer(size_t slot_no,
                                   size_t first_timepoint_idx,
                                   const typename Vectorizer::dataset_t& buffer,
                                   const std::vector<size_t>& lengths,
                                   int timepoints_to_analyze = -1)
  {
    int lock_count = InferenceEngine::get_lock_count(slot_no);
    // std::cerr << "GraphDpImpl::handle_token_buffer " << (slot_no+1) << ", "
    //           << first_timepoint_idx
    //           << ", lengths=" << lengths
    //           << ", " << timepoints_to_analyze
    //           << "; lock_count=" << lock_count << std::endl;
    send_results_if_available();
    acquire_slot(slot_no);
    // size_t offset = slot_no * buffer.size() + InferenceEngine::get_start_timepoint();
    size_t count = (timepoints_to_analyze > 0) ? timepoints_to_analyze : buffer.size();
    for (size_t i = 0; i < count; i++)
    {
      m_vectorizer.vectorize_timepoint(Matrix::get_tensor(), /*offset +*/ i, buffer[i]);
    }

    InferenceEngine::set_slot_lengths(slot_no, lengths);
    InferenceEngine::set_slot_begin(slot_no, first_timepoint_idx);
    InferenceEngine::set_slot_end(slot_no, /*offset +*/ count);

    auto& slot = InferenceEngine::m_slots[slot_no];

    lock_count = InferenceEngine::get_lock_count(slot_no);
    // std::cerr << "GraphDpImpl::handle_token_buffer slot " << (slot_no+1) << " retrieved: input="
    //           << slot.m_input_begin << ", " << slot.m_input_end << "; output="
    //           << slot.m_output_begin << ", " << slot.m_output_end << ", lock_count=" << lock_count
    //           << std::endl;

    InferenceEngine::start_job(slot_no, timepoints_to_analyze > 0);
    // std::cerr << "Slot " << slot_no << " sent to inference engine (graph_dp)" << std::endl;
  }

  inline void no_more_data(size_t slot_no)
  {
    if (!InferenceEngine::get_slot_started(slot_no))
    {
      while (InferenceEngine::get_lock_count(slot_no) > 1)
      {
        InferenceEngine::decrement_lock_count(slot_no);
      }
      InferenceEngine::start_job(slot_no, true);
    }
  }

protected:
  Vectorizer m_vectorizer;
  std::shared_ptr<FastTextVectorizer<typename Matrix::matrix_t, Eigen::Index>> m_fastText;
  std::vector<std::shared_ptr<FeatureVectorizerBase<Eigen::Index>>> m_featVectorizers;

  tagging_callback_t m_callback;

  uint64_t m_current_timepoint;
  uint32_t m_current_slot_timepoints;

  int32_t m_current_slot_no;
  int32_t m_last_completed_slot;

  size_t m_curr_buff_idx;
};

} // namespace impl
} // namespace graph_dp
} // namespace deeplima

#endif
