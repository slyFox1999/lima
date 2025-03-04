// Copyright 2021 CEA LIST
// SPDX-FileCopyrightText: 2022 CEA LIST <gael.de-chalendar@cea.fr>
//
// SPDX-License-Identifier: MIT

/************************************************************************
 * @author     Gaël de Chalendar <gael.de-chalendar@cea.fr>
 * @date       Wed Dec 15 2021
 ***********************************************************************/

#include "shiftFrom.h"
#include "qtSgmlEntities.h"

#include <QDebug>
#include <QFile>
#include <QRegularExpression>
#include <tuple>

namespace Lima {

class ShiftFromPrivate
{
public:
  ShiftFromPrivate(const QString& xml);
  ~ShiftFromPrivate() = default;

  void build_mapping();

  QMap<int, std::tuple<int, QString, QString> > m_shiftFrom;
  QString m_xml;
  QString m_xml_noent;
  QMap<int, QString> m_mapping;


};


const QString& ShiftFrom::xml()
{
  return m_d->m_xml;
}

const QString& ShiftFrom::xml_noent()
{
  return m_d->m_xml_noent;
}

/**
 * Replace in input the sequences in mapping (shifted by offset)
 *
 * This allows to put back interpreted entities that were replaced by
 * underscores to avoid introducing shifts in the input xml
 */
QString ShiftFrom::rebuild_text(const QString& input, int offset) const
{
  QString txt = input;
//   qDebug() << "Handling '" << txt << "' at offset" << offset;
//   qDebug() << "Mapping from" << offset << "to" << offset+txt.size();
  for (auto it = m_d->m_mapping.lowerBound(offset),
    it_end = m_d->m_mapping.upperBound(offset+txt.size());
       it != it_end; it++)
  {
//     qDebug() << "it:" << it.key();
    txt = txt.replace(it.key()-offset, it.value().size(), it.value());
  }
  return txt;
}

/**
 * Return the correct absolute position of a token in its origin text
 * when found at position @ref indexOfToken in a shifted text, taking into
 * account shiftings in @ref shiftFrom and the initial @ref offset.
 */
int ShiftFrom::correct_offset(int offset, int indexOfToken) const
{
//   qDebug() << "ShiftFrom::correct_offset("<<offset<<","<<indexOfToken<<")";
//   qDebug() << "ShiftFrom::correct_offset" << m_d;
//   qDebug() << "ShiftFrom::correct_offset" << m_d->m_shiftFrom.keys();
  auto correction = (
    m_d->m_shiftFrom.lowerBound(offset+indexOfToken-1)==m_d->m_shiftFrom.begin()
      ?0
      :(std::get<0>((m_d->m_shiftFrom.lowerBound(offset+indexOfToken-1)-1).value()))
                     );
//   qDebug() << "correction:" << correction;
  auto correctedOffset = offset + correction;
//   qDebug() << "corrected offset:" << correctedOffset;
  auto correctedPosition = correctedOffset+indexOfToken;
//   qDebug() << "corrected position:" << correctedPosition;
//   qDebug() << "";
  return correctedPosition;
}

ShiftFrom::ShiftFrom(const QString& xml):
  m_d(new ShiftFromPrivate(xml))
{
//   qDebug() << "ShiftFrom::ShiftFrom" << xml;
}


ShiftFrom::~ShiftFrom()
{
  delete m_d;
}

ShiftFromPrivate::ShiftFromPrivate(const QString& xml):
  m_xml(xml)
// QMap<int, std::tuple<int, QString, QString> > buildShiftFrom(const QString& xml)
{
//     qDebug() << "ShiftFromPrivate::ShiftFromPrivate" << m_xml;
    QRegularExpression rx("(&([^;]*);)");
    int shift = 0;

    qsizetype indexofent = 0;
    QRegularExpressionMatch match;
    while ((indexofent = m_xml.indexOf(rx, indexofent, &match)) != -1)
    {
//       qDebug() << indexofent;
//       qDebug() << rx.cap(1);
      int indexInResolved = indexofent-shift;
      QString entity = match.captured(1);
      QString entityString = match.captured(2);
//       qDebug() << entity;
      QString parsedEntity = parseEntity(entityString);
      shift += entity.size()-parsedEntity.size();
//       qDebug() << parsedEntity;
      m_shiftFrom.insert(indexInResolved, {shift, entity, parsedEntity});
//       qDebug() << "ShiftFromPrivate::ShiftFromPrivate indexofent:" << indexofent << "; indexInResolved:"
//               << indexInResolved << "; shift:" << shift;

      indexofent += match.capturedLength();
    }
//     qDebug() << "ShiftFromPrivate::ShiftFromPrivate shiftFrom is:" << m_shiftFrom.keys();

    build_mapping();
}

void ShiftFromPrivate::build_mapping()
{
//     qDebug() << m_xml;
    QRegularExpression rx("(&([^;]*);)");
    m_xml_noent = m_xml;

    qsizetype indexofent = 0;
    QRegularExpressionMatch match;
    while ((indexofent = m_xml_noent.indexOf(rx, indexofent, &match)) != -1)
    {
      auto entity = match.captured(1);
      auto entityString = match.captured(2);
      auto parsedEntity = parseEntity(entityString);
      m_xml_noent.replace(indexofent, entity.size(),
                        QString(parsedEntity.size(),'_'));
      m_mapping.insert(indexofent, parsedEntity);
      indexofent += parsedEntity.size();
    }
//     qDebug() << "xml_noent:" << m_xml_noent;
//     qDebug() << "mapping:" << m_mapping;
}


QDebug& operator<<(QDebug& os, const ShiftFrom& sf)
{
//     QMap<int, std::tuple<int, QString, QString> > m_shiftFrom;
  os << "ShiftFrom xml:" << sf.m_d->m_xml << QTENDL;
  os << "ShiftFrom xml_noent:" << sf.m_d->m_xml_noent << QTENDL;
  os << "ShiftFrom xml rebuilt:" << sf.rebuild_text(sf.m_d->m_xml_noent, 0) << QTENDL;
  os << "ShiftFrom shiftFrom: {" ;
  for (const auto& k: sf.m_d->m_shiftFrom.keys())
  {
    os << "{" << k << ":" << "{" << std::get<0>(sf.m_d->m_shiftFrom[k])
        << "," << std::get<1>(sf.m_d->m_shiftFrom[k])
        << "," << std::get<2>(sf.m_d->m_shiftFrom[k])
        << "}" << "}";
  }
  os << "}" ;
  return os;
}


} // end namespace
