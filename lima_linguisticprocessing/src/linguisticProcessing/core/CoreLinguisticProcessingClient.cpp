// Copyright 2004-2021 CEA LIST
// SPDX-FileCopyrightText: 2022 CEA LIST <gael.de-chalendar@cea.fr>
//
// SPDX-License-Identifier: MIT

#ifdef WIN32
#define _WINSOCKAPI_
#endif

#ifndef WIN32
#include <cstdint> //uint32_t
#endif

#include "CoreLinguisticProcessingClient.h"

#include "common/Data/strwstrtools.h"
#include "common/MediaProcessors/MediaProcessors.h"
#include "common/MediaProcessors/MediaProcessUnitPipeline.h"
#include "common/MediaticData/mediaticData.h"
#include "common/ProcessUnitFramework/AnalysisContent.h"
#include "common/XMLConfigurationFiles/xmlConfigurationFileExceptions.h"
#include "common/time/timeUtilsController.h"
#include "common/tools/FileUtils.h"
#include "linguisticProcessing/LinguisticProcessingCommon.h"
#include "linguisticProcessing/client/LinguisticProcessingClientFactory.h"
#include "linguisticProcessing/common/linguisticData/LimaStringText.h"
#include "linguisticProcessing/core/LinguisticProcessors/LinguisticMetaData.h"
#include "linguisticProcessing/core/LinguisticResources/LinguisticResources.h"

#include <QDate>
#include <QFileInfo>
#include <QRegularExpression>

uint64_t t1;

using namespace std;
using namespace Lima::Common::MediaticData;
using namespace Lima::Common::XMLConfigurationFiles;

//using namespace boost;

uint64_t t2;

namespace Lima
{

namespace LinguisticProcessing
{
std::unique_ptr<CoreLinguisticProcessingClientFactory> CoreLinguisticProcessingClientFactory::s_instance=std::unique_ptr<CoreLinguisticProcessingClientFactory>(new CoreLinguisticProcessingClientFactory());


CoreLinguisticProcessingClient::CoreLinguisticProcessingClient()
{
  //CORECLIENTLOGINIT;
  //LERROR << "CoreLinguisticProcessingClient::CoreLinguisticProcessingClient()";
}

CoreLinguisticProcessingClient::~CoreLinguisticProcessingClient()
{
  //CORECLIENTLOGINIT;
  //LERROR << "CoreLinguisticProcessingClient::~CoreLinguisticProcessingClient()";
}

std::shared_ptr<AnalysisContent> CoreLinguisticProcessingClient::analyze(
    const std::string& text,
    const std::map<std::string,std::string>& metaData,
    const std::string& pipelineId,
    const std::map<std::string, AbstractAnalysisHandler*>& handlers,
    const std::set<std::string>& inactiveUnits) const

{
  auto limatext = QString::fromStdString(text);

  return analyze(limatext, metaData, pipelineId, handlers, inactiveUnits);
}

std::shared_ptr<AnalysisContent> CoreLinguisticProcessingClient::analyze(
    const QString& text,
    const std::map<std::string,std::string>& metaData,
    const std::string& pipelineId,
    const std::map<std::string, AbstractAnalysisHandler*>& handlers,
    const std::set<std::string>& inactiveUnits) const

{
  Lima::TimeUtilsController timer("CoreLinguisticProcessingClient::analyze");
  CORECLIENTLOGINIT;

 //if (text.isEmpty())

  QString whitespaceOnly("\\s*");

  whitespaceOnly = QRegularExpression::anchoredPattern(whitespaceOnly);
  auto analysis = std::make_shared<AnalysisContent>();
  if (QRegularExpression(whitespaceOnly).match(text).hasMatch())
  {
    LWARN << "Empty text given to LIMA linguistic processing client. Nothing to do.";
    return analysis;
  }

  // create analysis content
  LinguisticMetaData* metadataholder=new LinguisticMetaData(); // will be destroyed in AnalysisContent destructor
  analysis->setData("LinguisticMetaData",metadataholder);

  metadataholder->setMetaData(metaData);
  LimaStringText* lstexte=new LimaStringText(text); // will be destroyed in AnalysisContent destructor
  analysis->setData("Text", lstexte);

  LINFO << "CoreLinguisticProcessingClient::analyze(";
  for(auto attrIt = metaData.cbegin() ; attrIt != metaData.cend() ; attrIt++ ) {
    LINFO << "attr:" << attrIt->first << "value:" << attrIt->second << ", " ;
  }
  LINFO;

  // add date/time/location metadata in LinguisticMetaData
#ifdef DEBUG_LP
  if (metaData.empty()) {
    LDEBUG << "CoreLinguisticProcessingClient::analyze: no metadata";
  }
#endif
  for (auto it=metaData.cbegin(), it_end=metaData.cend(); it!=it_end; it++)
  {
    if ((*it).first=="date")
    {
      try {
        const auto& str = (*it).second;
        auto i = str.find("T"); //2006-12-11T12:44:00
        /*if (i!=std::string::npos) {
          QTime docTime=posix_time::time_from_string(str);
          metadataholder->setTime("document",docTime);
          LDEBUG << "use '"<< str << "' as document time";
          }*/
        std::string date(str, 0, i);
        auto docDate = QDate::fromString(date.c_str(),Qt::ISODate);
        metadataholder->setDate("document",docDate);

#ifdef DEBUG_LP
        LDEBUG << "use '"<< date << "' as document date";
        LDEBUG << "use boost'"<< docDate.day() <<"/"<< docDate.month() <<"/"<< docDate.year() << "' as document date";
#endif
      }
      catch (std::exception& e)
      {
        LERROR << "Error in date conversion (date '"<< (*it).second
               << "' will be ignored): " << e.what();
      }
    }
    else if ((*it).first=="location")
    {
      metadataholder->setLocation("document",(*it).second);
#ifdef DEBUG_LP
        LDEBUG << "use '"<< (*it).second<< "' as document location";
#endif
    }
    else if ((*it).first=="time")
    {
      try {
        auto docTime= QTime::fromString((*it).second.c_str(),"hh:mm:ss.z" );
        metadataholder->setTime("document",docTime);
#ifdef DEBUG_LP
        LDEBUG << "use '"<< (*it).second<< "' as document time";
#endif
      }
      catch (std::exception& e)
      {
        LERROR << "Error in ptime conversion (time '"<< (*it).second
               << "' will be ignored): " << e.what();
      }
    }
    else if ((*it).first=="docid")
    {
#ifdef DEBUG_LP
      LDEBUG << "use '"<< (*it).second<< "' as document id";
#endif
      metadataholder->setMetaData("DocId",(*it).second);
    }
  }

  std::string docId;
  try
  {
    docId = metadataholder->getMetaData("DocId");
  }
  catch (LinguisticProcessingException& )
  {
    metadataholder->setMetaData("DocId", docId);
  }
  LINFO << "CoreLinguisticProcessingClient::analyze(" << docId << "...)";


  // try to retrieve offset
  try
  {
    const auto& offsetStr = metadataholder->getMetaData("StartOffset");
    metadataholder->setStartOffset(atoi(offsetStr.c_str()));
  }
  catch (LinguisticProcessingException& )
  {
    metadataholder->setStartOffset(0);
  }

  const auto& fileName = metadataholder->getMetaData("FileName");
  // get language
  const auto& lang = metadataholder->getMetaData("Lang");
  LINFO  << "analyze file is: '" << fileName << "'";
  LINFO  << "analyze pipeline is '" << pipelineId << "'";
  LINFO  << "analyze language is '" << lang << "'";
#ifdef DEBUG_LP
  LDEBUG << "text : " << text;
#endif
  //LDEBUG << "text : " << Common::Misc::limastring2utf8stdstring(texte);

  auto langId = MediaticData::single().getMediaId(lang);

  // get pipeline
  auto pipeline = MediaProcessors::single().getPipelineForId(langId,pipelineId);
  if (pipeline == nullptr)
  {
    LERROR << "can't get pipeline '" << pipelineId << "'";
    throw LinguisticProcessingException(
      std::string("can't get pipeline '" + pipelineId + "' for language '" + lang + "'") );
  }
  InactiveUnitsData* inactiveUnitsData = new InactiveUnitsData();
  for (auto inactiveUnit: inactiveUnits)
  {
//     const_cast<MediaProcessUnitPipeline*>(pipeline)->setInactiveProcessUnit(*it);
    inactiveUnitsData->insert(inactiveUnit);
  }
  analysis->setData("InactiveUnits", inactiveUnitsData);

  // add handler to analysis
#ifdef DEBUG_LP
  LDEBUG << "add handler to analysis" ;
  for (auto hit = handlers.begin(); hit != handlers.end(); hit++)
  {
    LDEBUG << "    " << (*hit).first << (*hit).second;
  }
#endif
  auto h = new AnalysisHandlerContainer(const_cast<std::map<std::string, AbstractAnalysisHandler*>& >(handlers));
#ifdef DEBUG_LP
  LDEBUG << "set data" ;
#endif
  analysis->setData("AnalysisHandlerContainer", h);

  // process analysis
#ifdef DEBUG_LP
  LDEBUG << "Process pipeline..." ;
#endif
  auto status = pipeline->process(*analysis);
#ifdef DEBUG_LP
  LDEBUG << "pipeline process returned status " << (int)status ;
#endif
  if (status!=SUCCESS_ID)
  {
      LIMA_LP_EXCEPTION( "analysis failed : receive status " << (int)status
                         << " from pipeline." );
  }
  return analysis;
}

CoreLinguisticProcessingClientFactory::CoreLinguisticProcessingClientFactory() :
  AbstractLinguisticProcessingClientFactory("lima-coreclient")
{
  //std::cerr << "CoreLinguisticProcessingClientFactory::CoreLinguisticProcessingClientFactory()" << std::endl;
  //std::cerr << "    calling AbstractLinguisticProcessingClientFactory(\"lima-coreclient\")" << std::endl;
}

CoreLinguisticProcessingClientFactory::~CoreLinguisticProcessingClientFactory()
{
  //std::cerr << "CoreLinguisticProcessingClientFactory::~CoreLinguisticProcessingClientFactory()" << std::endl;
}

void CoreLinguisticProcessingClientFactory::configure(
  Common::XMLConfigurationFiles::XMLConfigurationFileParser& configuration,
  std::deque<std::string> langs,
  std::deque<std::string> pipelines)
{
  Lima::TimeUtilsController timer("LPCoreClientInit");
  LPCLIENTFACTORYLOGINIT;
  LINFO << "CoreLinguisticProcessingClientFactory::configure";

  // initialize some entity types internally used in linguistic processing
  Common::MediaticData::MediaticData::changeable().initEntityTypes(configuration);

  deque<string> langToload(langs);
  if (langToload.empty())
  {
    try
    {
      langToload = configuration.getModuleGroupListValues("lima-coreclient",
                                                          "mediaProcessingDefinitionFiles",
                                                          "available");
    }
    catch (NoSuchList& )
    {
      LERROR << "no parameter lima-coreclient/mediaProcessingDefinitionFiles/available !";
      throw InvalidConfiguration("no parameter lima-coreclient/mediaProcessingDefinitionFiles/available !");
    }
  }

  for (const auto& lang: langToload)
  {
    LINFO << "CoreLinguisticProcessingClientFactory::configure load language " << lang;
    auto langid = MediaticData::single().getMediaId(lang);
    QString file;
    try
    {
      auto configPaths = QString::fromUtf8(Common::MediaticData::MediaticData::single().getConfigPath().c_str()).split(LIMA_PATH_SEPARATOR);
      if (configPaths.isEmpty())
      {
        LERROR << "no config paths available in MediaticData";
        throw InvalidConfiguration("no config paths available in MediaticData");
      }
      auto mediaProcessingDefinitionFile = QString::fromUtf8(configuration.getModuleGroupParamValue(
            "lima-coreclient",
            "mediaProcessingDefinitionFiles",
            lang).c_str());
      Q_FOREACH(QString confPath, configPaths)
      {
        if  (QFileInfo::exists(confPath + "/" + mediaProcessingDefinitionFile))
        {
          file = confPath + "/" + mediaProcessingDefinitionFile;
          break;
        }
      }
      if (file.isEmpty())
      {
        LERROR << "no language definition file"<< mediaProcessingDefinitionFile
                << "for language" << lang << "found in config paths"
                << configPaths;
        throw InvalidConfiguration("no language definition file for language ");
      }
    }
    catch (NoSuchParam& )
    {
      LERROR << "no language definition file for language " << lang;
      throw InvalidConfiguration("no language definition file for language ");
    }
    XMLConfigurationFileParser langParser(file);

    //initialize SpecificEntities
    Common::MediaticData::MediaticData::changeable().initEntityTypes(langParser);

    // initialize resources
    LINFO << "configure resources for language " << lang;
    try
    {
      auto& module = langParser.getModuleConfiguration("Resources");
      LinguisticResources::changeable().initLanguage(
        langid,
        module,
        lang.find("ud-") != 0 && lang != "ud"); // load main keys for non ud languages
    }
    catch (NoSuchModule& )
    {
      LERROR << "no module 'Resources' in configuration file " << file;
      throw InvalidConfiguration("no module 'Resources' in configuration file ");
    }

    // initialize processors
    LINFO << "initialize processors";
    try
    {
      auto& procmodule = langParser.getModuleConfiguration("Processors");
      MediaProcessors::changeable().initMedia(
        langid,
        procmodule/*,
        dumpmodule*/);
    }
    catch (NoSuchModule& )
    {
      LERROR << "missing module 'Processors' in language configuration file " << file;
      throw InvalidConfiguration("missing module 'Processors' in language configuration file ");
    }

  }

  LINFO << "initialize Pipelines";
  try
  {
    auto& group = configuration.getModuleGroupConfiguration("lima-coreclient","pipelines");
    MediaProcessors::changeable().initPipelines(group,pipelines);
  }
  catch (NoSuchModule& )
  {
    LERROR << "no module 'pipelines' in lima-analysis.xml (configuration file)";
    throw InvalidConfiguration("no module 'pipelines' in mm-LP.xml (configuration file)");
  }
}

std::shared_ptr< AbstractProcessingClient > CoreLinguisticProcessingClientFactory::createClient() const
{
  return std::shared_ptr< AbstractProcessingClient >(new CoreLinguisticProcessingClient());
}


} // LinguisticProcessing

} // Lima
