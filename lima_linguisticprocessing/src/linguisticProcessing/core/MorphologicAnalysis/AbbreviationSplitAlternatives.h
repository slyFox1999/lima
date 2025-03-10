// Copyright 2002-2013 CEA LIST
// SPDX-FileCopyrightText: 2022 CEA LIST <gael.de-chalendar@cea.fr>
//
// SPDX-License-Identifier: MIT

/**
  * @brief  AbbreviationSplitAlternatives is the module which creates split alternatives
  *         for hyphen word tokens. Each token from the supplied tokens path is processed :
  *         o FullToken must be "AlphaHyphen" typed by Tokenizer.
  *         o If a token has a single word entry or an orthographic alternative
  *           it is not decomposed
  *         o Token is break at hyphen boundaries and a new alternative path is created
  *         o each FullToken of the new Path is searched into dictionnary as Simple Word
  *         o If special hyphen entry, no alternatives are searched,
  *           otherwise Accented alternatives are searched
  *         o Path is valid even if not all FullToken have entry into dictionary
  *         @br
  *         Modified @date Dec, 02 2002 by GC to handle splitting on t_alpha_possessive
  *
  * @file   AbbreviationSplitAlternatives.h
  * @author NAUTITIA jys
  * @author Gael de Chalendar
  * @author Copyright (c) 2002-2003 by CEA
  *
  * @date   created on Nov, 30 2002
  * @version    $Id$
  *
  */

#ifndef LIMA_MORPHOLOGICANALYSIS_HYPHENWORDALTERNATIVES_H
#define LIMA_MORPHOLOGICANALYSIS_HYPHENWORDALTERNATIVES_H

#include "MorphologicAnalysisExport.h"
#include "common/MediaProcessors/MediaProcessUnit.h"
#include "linguisticProcessing/core/LinguisticAnalysisStructure/LinguisticGraph.h"
#include "linguisticProcessing/core/FlatTokenizer/Tokenizer.h"
#include "linguisticProcessing/core/AnalysisDict/AbstractAnalysisDictionary.h"
#include "AlternativesReader.h"

#include <QRegularExpression>

namespace Lima {
  namespace Common {
    namespace AnnotationGraphs {
      class AnnotationData;
    }
  }
  namespace LinguisticProcessing {
namespace MorphologicAnalysis {

#define ABBREVIATIONSPLITALTERNATIVESFACTORY_CLASSID "AbbreviationSplitAlternatives"

class LIMA_MORPHOLOGICANALYSIS_EXPORT AbbreviationSplitAlternatives : public MediaProcessUnit
{

public:
    AbbreviationSplitAlternatives();
    virtual ~AbbreviationSplitAlternatives();

  void init(
    Common::XMLConfigurationFiles::GroupConfigurationStructure& unitConfiguration,
    Manager* manager) override
  ;

  LimaStatusCode process(
    AnalysisContent& analysis) const override;

protected:

private:

    std::shared_ptr<FlatTokenizer::Tokenizer> m_tokenizer;
    std::shared_ptr<AnalysisDict::AbstractAnalysisDictionary> m_dictionary;
    std::vector<LimaString> m_abbreviations;
    MediaId m_language;
    bool m_confidentMode;
    std::shared_ptr<AlternativesReader> m_reader;
    QRegularExpression m_charSplitRegexp;

    bool makeConcatenatedAbbreviationSplitAlternativeFor(
        LinguisticGraphVertex splitted,
        LinguisticGraph* graph,
        Common::AnnotationGraphs::AnnotationData* annotationData) const;

    bool makePossessiveAlternativeFor(
        LinguisticGraphVertex splitted,
        LinguisticGraph* graph,
        Common::AnnotationGraphs::AnnotationData* annotationData) const;
};

} // closing namespace MorphologicAnalysis
} // closing namespace LinguisticProcessing
} // closing namespace Lima

#endif // LIMA_MORPHOLOGICANALYSIS_HYPHENWORDALTERNATIVES_H
