// Copyright 2002-2021 CEA LIST
// SPDX-FileCopyrightText: 2022 CEA LIST <gael.de-chalendar@cea.fr>
//
// SPDX-License-Identifier: MIT

#include "PythonTensorFlowTokenizer.h"
#include "linguisticProcessing/common/helpers/DeepTokenizerBase.h"
#include "PythonHelpers.h"

#include "linguisticProcessing/core/LinguisticResources/LinguisticResources.h"
#include "linguisticProcessing/common/linguisticData/LimaStringText.h"
#include "linguisticProcessing/core/LinguisticAnalysisStructure/AnalysisGraph.h"
#include "linguisticProcessing/core/TextSegmentation/SegmentationData.h"
#include "common/misc/Exceptions.h"
#include "common/Data/strwstrtools.h"
#include "common/AbstractFactoryPattern/SimpleFactory.h"
#include "common/XMLConfigurationFiles/xmlConfigurationFileExceptions.h"
#include "common/tools/FileUtils.h"
#include "common/MediaticData/mediaticData.h"
#include "common/time/timeUtilsController.h"

#include <QtCore/QTemporaryFile>
#include <QtCore/QRegularExpression>

#include <string>

using namespace std;
using namespace Lima::LinguisticProcessing::LinguisticAnalysisStructure;
using namespace Lima::Common::XMLConfigurationFiles;
using namespace Lima::Common::Misc;

namespace Lima
{
namespace LinguisticProcessing
{
namespace TensorFlowUnits
{
namespace Tokenizer
{

static SimpleFactory<MediaProcessUnit,PythonTensorFlowTokenizer> pythontokenizerFactory(PYTHONTENSORFLOWTOKENIZER_CLASSID); // clazy:exclude=non-pod-global-static

class PythonTokenizerPrivate : public DeepTokenizerBase
{
public:
  PythonTokenizerPrivate();
  virtual ~PythonTokenizerPrivate();

  MediaId m_language;
  PyObject* m_instance;
  FsaStringsPool* m_stringsPool;
  LinguisticGraphVertex m_currentVx;
  QString m_data;

};

PythonTokenizerPrivate::PythonTokenizerPrivate() :
    m_instance(nullptr),
    m_stringsPool(nullptr),
    m_currentVx(0)
{
}

PythonTokenizerPrivate::~PythonTokenizerPrivate()
{
}

PythonTensorFlowTokenizer::PythonTensorFlowTokenizer() : m_d(new PythonTokenizerPrivate())
{
}

PythonTensorFlowTokenizer::~PythonTensorFlowTokenizer()
{
  Py_DECREF(m_d->m_instance);

  // Finish the Python Interpreter
  Py_Finalize();
  delete m_d;
}

void PythonTensorFlowTokenizer::init(
    GroupConfigurationStructure& unitConfiguration,
    Manager* manager)

{
#ifdef DEBUG_LP
  TOKENIZERLOGINIT;
  LDEBUG << "PythonTensorFlowTokenizer::init";
#endif
  m_d->m_language = manager->getInitializationParameters().media;
  m_d->m_stringsPool = &Common::MediaticData::MediaticData::changeable().stringsPool(m_d->m_language);

 try
  {
    m_d->m_data = QString::fromUtf8(unitConfiguration.getParamsValueAtKey("data").c_str());
  }
  catch (NoSuchParam& )
  {
    m_d->m_data = QString::fromUtf8("SentenceBoundaries");
  }

  QString path; // The path to the LIMA python tensorflow-based tokenizer
  try
  {
    path = QString::fromUtf8(unitConfiguration.getParamsValueAtKey("path").c_str());
  }
  catch (NoSuchParam& )
  {
    LIMA_EXCEPTION_SELECT_LOGINIT(
      TOKENIZERLOGINIT,
      "no param 'path' in TensorFlowTokenizer group configuration",
      InvalidConfiguration);
  }

  QString udCorpus; // The name/id of the Universal Dependencies corpus used for training
  try
  {
    udCorpus = QString::fromUtf8(unitConfiguration.getParamsValueAtKey("corpus").c_str());
  }
  catch (NoSuchParam& )
  {
    LIMA_EXCEPTION_SELECT_LOGINIT(
      TOKENIZERLOGINIT,
      "no param 'corpus' in PythonTensorFlowTokenizer group configuration",
      InvalidConfiguration);
  }

  QString embeddingsPath; // The path to the LIMA python tensorflow-based tokenizer
  try
  {
    embeddingsPath = QString::fromUtf8(unitConfiguration.getParamsValueAtKey("embeddings_path").c_str());
  }
  catch (NoSuchParam& )
  {
    LIMA_EXCEPTION_SELECT_LOGINIT(
      TOKENIZERLOGINIT,
      "no param 'embeddings_path' in PythonTensorFlowTokenizer group configuration",
      InvalidConfiguration);
  }

  QString modelPath; // The path to the LIMA python tensorflow-based tokenizer
  try
  {
    modelPath = QString::fromUtf8(unitConfiguration.getParamsValueAtKey("model_path").c_str());
  }
  catch (NoSuchParam& )
  {
    LIMA_EXCEPTION_SELECT_LOGINIT(
      TOKENIZERLOGINIT,
      "no param 'model_path' in PythonTensorFlowTokenizer group configuration",
      InvalidConfiguration);
  }

  int windowSize = -1; // The window size used with the tensorflow-based tokenizer
  try
  {
    bool success;
    windowSize = QString::fromUtf8(unitConfiguration.getParamsValueAtKey("window_size").c_str()).toInt(&success);
    if (!success)
    {
      LIMA_EXCEPTION_SELECT_LOGINIT(
        TOKENIZERLOGINIT,
        "Param 'window_size' in PythonTensorFlowTokenizer group configuration is not an integer",
        InvalidConfiguration);
    }
  }
  catch (NoSuchParam& )
  {
    LIMA_EXCEPTION_SELECT_LOGINIT(
      TOKENIZERLOGINIT,
      "no param 'window_size' in PythonTensorFlowTokenizer group configuration",
      InvalidConfiguration);
  }

  int batchSize = -1; // The bach size used with the tensorflow-based tokenizer
  try
  {
    bool success;
    batchSize = QString::fromUtf8(unitConfiguration.getParamsValueAtKey("batch_size").c_str()).toInt(&success);
    if (!success)
    {
      LIMA_EXCEPTION_SELECT_LOGINIT(
        TOKENIZERLOGINIT,
        "Param 'batch_size' in PythonTensorFlowTokenizer group configuration is not an integer",
        InvalidConfiguration);
    }
  }
  catch (NoSuchParam& )
  {
    LIMA_EXCEPTION_SELECT_LOGINIT(
      TOKENIZERLOGINIT,
      "no param 'batch_size' in PythonTensorFlowTokenizer group configuration",
      InvalidConfiguration);
  }


  // Initialize the python SRL system
  // Find the first python executable in the path and use it as the program name.
  // This allows to find the modules set up in an activated virtualenv
  QString str_program_name;
  QString pathEnv = QString::fromUtf8(qgetenv("PATH").constData());
  QStringList paths = pathEnv.split(QRegularExpression("[;:]"));
  for (auto it = paths.begin(); it != paths.end(); ++it)
  {
    const auto & path = *it;
    if (QFile::exists(path + "/python" ))
    {
      str_program_name = path + "/python";
      break;
    }
  }
#ifndef WIN32
  Py_SetProgramName(const_cast<wchar_t*>( str_program_name.toStdWString().c_str()));
#else
  Py_SetProgramName( (wchar_t*)str_program_name.unicode() );
#endif


  Py_Initialize();

  PyObject* main_module = PyImport_ImportModule("__main__");
  PyObject* main_dict = PyModule_GetDict(main_module);
  PyObject* sys_module = PyImport_ImportModule("sys");
  if (sys_module == nullptr)
  {
    failed_to_import_the_sys_module();
    Py_Exit(1);
  }

  PyDict_SetItemString(main_dict, "sys", sys_module);

  // Add the path to the knowledgesrl pachkage to putho path
  PyObject* pythonpath = PySys_GetObject("path");
  if (PyList_Append(pythonpath, PyUnicode_DecodeFSDefault(path.toUtf8().constData())) ==  -1)
  {
    TOKENIZERLOGINIT;
    LERROR << "Failed to append to python path";
    PyErr_Print();
    Py_Exit(1);
  }

  // Import the tokenizer module
  PyObject* tokenizer_module = PyImport_ImportModule("tfudpipe-predict");
  if (tokenizer_module == NULL)
  {
    TOKENIZERLOGINIT;
    LERROR << "PythonTensorFlowTokenizer::init"<< __FILE__ << __LINE__
            << ": Failed to import tokenizer module";
    PyErr_Print();
    Py_Exit(1);
  }

  // pDict is a borrowed reference
  auto pDict = PyModule_GetDict(tokenizer_module);

  // Build the name of a callable class
  auto pClass = PyDict_GetItemString(pDict, "Tokenizer");
  // Create an instance of the class
  if (PyCallable_Check(pClass))
  {
    //     corpus, embeddings_path, model_path, window_size
    auto corpus = PyUnicode_FromString(udCorpus.toUtf8().constData());
    if (corpus == NULL)
    {
      failed_to_allocate_memory();
    }
    auto embeddings_path = PyUnicode_FromString(embeddingsPath.toUtf8().constData());
    if (embeddings_path == NULL)
    {
      failed_to_allocate_memory();
    }
    auto model_path = PyUnicode_FromString(modelPath.toUtf8().constData());
    if (model_path == NULL)
    {
      failed_to_allocate_memory();
    }
    auto window_size = PyLong_FromLong(windowSize);
    if (window_size == NULL)
    {
      failed_to_allocate_memory();
    }
    auto batch_size = PyLong_FromLong(batchSize);
    if (batch_size == NULL)
    {
      failed_to_allocate_memory();
    }

    auto pArgs = PyTuple_New(5);
    if (pArgs == NULL)
    {
      failed_to_allocate_memory();
    }
    if (PyTuple_SetItem(pArgs, 0, corpus) != 0
        || PyTuple_SetItem(pArgs, 1, embeddings_path) != 0
        || PyTuple_SetItem(pArgs, 2, model_path) != 0
        || PyTuple_SetItem(pArgs, 3, window_size) != 0
        || PyTuple_SetItem(pArgs, 4, batch_size) != 0)
    {
      python_error();
    }

    m_d->m_instance = PyObject_CallObject(pClass, pArgs);
    if (m_d->m_instance == NULL)
    {
      cannot_instantiate_the_tokenizer_python_class();
    }
#ifdef DEBUG_LP
    TOKENIZERLOGINIT;
    LDEBUG << "PythonTensorFlowTokenizer::init";
#endif

    Py_DECREF(corpus);
    Py_DECREF(embeddings_path);
    Py_DECREF(model_path);
    Py_DECREF(window_size);
    Py_DECREF(batch_size);
  }
  else
  {
    TOKENIZERLOGINIT;
    LERROR << "Was not able to retrieve the python class to instantiate.";
    PyErr_Print();
    Py_Exit(1);
  }
}

LimaStatusCode PythonTensorFlowTokenizer::process(AnalysisContent& analysis) const
{
  TimeUtilsController TensorFlowTokenizerProcessTime("TensorFlowTokenizer");
  TOKENIZERLOGINIT;
  LINFO << "start tokenizer process";
  auto anagraph = new AnalysisGraph("AnalysisGraph",m_d->m_language,true,true);
  analysis.setData("AnalysisGraph",anagraph);
  LinguisticGraph* graph=anagraph->getGraph();
  m_d->m_currentVx = anagraph->firstVertex();
  // Get text from analysis
  auto originalText = std::dynamic_pointer_cast<LimaStringText>(analysis.getData("Text"));
  // Convert text to PyObject
  auto pyText = PyUnicode_FromString(originalText->toUtf8().constData());
  if (pyText == NULL)
  {
    failed_to_allocate_memory();
  }
  auto pyTokenize = PyUnicode_FromString("tokenize");
  if (pyTokenize == NULL)
  {
    failed_to_allocate_memory();
  }
  // Call python tokenizer object tokenize method with the text
  auto pySentences = PyObject_CallMethodObjArgs(m_d->m_instance,
                                                 pyTokenize,
                                                 pyText,
                                                 NULL);
  if (pySentences == NULL)
  {
    python_error();
  }
  // Convert resulting python list of list of tuples (pairs) to Qt or std
  // objects
  auto sentencesPyTokens = pyListOrTupleToVector(pySentences);
  std::vector< std::vector< std::pair<QString, int> > > sentencesTokens;
  for (auto& sentencePyTokens: sentencesPyTokens)
  {
    std::vector< std::pair<QString, int> > sentence;
    auto sentenceTokensPyPair = pyListOrTupleToVector(sentencePyTokens);
    for (auto& pyPair: sentenceTokensPyPair)
    {
      // pyPair is a PyObject which is a PyTuple with two elements: a PyUnicode and a PyLong
      auto pyToken = pyListOrTupleToVector(pyPair);
      // pyToken is a std::vector with two elements: a PyUnicode and a PyLong
      auto pyStr = PyUnicode_AsEncodedString(pyToken[0], "utf-8", "strict");
      if (pyStr == NULL)
      {
        python_error();
      }
      auto strExcType =  PyBytes_AsString(pyStr);
      if (strExcType == NULL)
      {
        python_error();
      }
      auto tokenString = QString::fromUtf8(strExcType);
      if (tokenString.isEmpty())
      {
        continue;
      }
      Py_XDECREF(pyStr);

      auto offset = PyLong_AsLong(pyToken[1]);
      if (offset == -1 && PyErr_Occurred())
      {
        python_error();
      }
      auto token = std::make_pair(tokenString, offset);

      sentence.push_back(token);
    }

    sentencesTokens.push_back(sentence);
  }
  Py_DECREF(pyText);
  Py_DECREF(pyTokenize);

  SegmentationData* sb = new SegmentationData("AnalysisGraph");
  analysis.setData(m_d->m_data.toUtf8().constData(), sb);

#ifdef DEBUG_LP
    LDEBUG << "PythonTensorFlowTokenizer::process removing edge"
            << anagraph->firstVertex() << anagraph->lastVertex();
#endif
  remove_edge(anagraph->firstVertex(),
              anagraph->lastVertex(),
              *graph);

  // Insert the tokens in the graph and create sentence limits
  LinguisticGraphVertex beginSentence = 0;
  for (const auto& sentence: sentencesTokens)
  {
    LinguisticGraphVertex endSentence = std::numeric_limits< LinguisticGraphVertex >::max();
    for (const auto& token: sentence)
    {
      const auto& str = token.first;
#ifdef DEBUG_LP
      LDEBUG << "      Adding token '" << str << "'";
#endif
      StringsPoolIndex form=(*m_d->m_stringsPool)[str];
      Token *tToken = new Token(form,str, token.second, token.first.size());
      if (tToken == 0) throw MemoryErrorException();

      m_d->computeDefaultStatus(*tToken);
#ifdef DEBUG_LP
      //   LDEBUG << "      curSettings is " << curSettings.toString();
      LDEBUG << "      status is " << tToken->status().toString();
#endif

      // Adds on the path
      LinguisticGraphVertex newVx = add_vertex(*graph);
//       if (beginSentence == std::numeric_limits< LinguisticGraphVertex >::max())
//         beginSentence = newVx;
      endSentence = newVx;
      put(vertex_token, *graph, newVx, tToken);
      put(vertex_data, *graph, newVx, new MorphoSyntacticData());
      add_edge(m_d->m_currentVx, newVx, *graph);
      m_d->m_currentVx = newVx;
    }
#ifdef DEBUG_LP
    LDEBUG << "adding sentence" << beginSentence << endSentence;
#endif
    sb->add(Segment("sentence", beginSentence, endSentence, anagraph));
    beginSentence = endSentence;
  }

  add_edge(m_d->m_currentVx,anagraph->lastVertex(),*graph);
  return SUCCESS_ID;
}

} // namespace Tokenizer
} // namespace TensorFlowUnits
} // namespace LinguisticProcessing
} // namespace Lima
