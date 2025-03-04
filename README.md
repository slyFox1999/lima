LIMA - Libre Multilingual Analyzer
==================================
![LIMA logo](https://raw.githubusercontent.com/aymara/lima/master/pics/lima-logo.png)

# TL;DR

LIMA python bindings are currently available **under Linux only** (x86_64).

Under Linux with python >= 3.7 and < 4, and **upgraded pip**:

```bash
# Upgrading pip is fundamental in order to obtain the correct LIMA version
$ pip install --upgrade pip
$ pip install aymara==0.4.1
$ lima_models.py -l eng
$ python
>>> import aymara.lima
>>> nlp = aymara.lima.Lima("ud-eng")
>>> sentences = nlp('Hello, World!')
>>> print(sentences[0][0].lemma)
hello
>>> print(sentences.conll())
# sent_id = 1
# text = Hello, World!
1       Hello   hello   INTJ    _       _               0       root      _ Len=5|Pos=1|SpaceAfter=No
2       ,       ,       PUNCT   _       _               1       punct     _ Len=1|Pos=6
3       World   World   PROPN   _       Number=Sing     1       vocative  _ Len=5|Pos=8|SpaceAfter=No
4       !       !       PUNCT   _       _               1       punct     _ Len=1|Pos=13
```

# Introducing LIMA

LIMA is a multilingual linguistic analyzer developed by the [CEA LIST](http://www-list.cea.fr/en), [LASTI laboratory](http://www.kalisteo.fr/en/index.htm) (French acronym for Text and Image Semantic Analysis Laboratory). LIMA is Free Software, available under the MIT license.

LIMA has [state of the art performance for more than 60 languages](https://github.com/aymara/lima-models/blob/master/eval.md) thanks to its recent deep learning (neural network) based modules. But it includes also a very powerful rules based mechanism called ModEx allowing to quickly extract information (entities, relations, events…) in new domains where annotated data does not exist.

For more information, installation instructions and documentation, please refer to [the LIMA Wiki](https://github.com/aymara/lima/wiki).

<!---
Drone.io Build Status: [![Drone.io Build Status](https://drone.io/github.com/aymara/lima/status.png)](https://drone.io/github.com/aymara/lima/latest)
-->

[![Build status](https://ci.appveyor.com/api/projects/status/github/aymara/lima?branch=master&svg=true)](https://ci.appveyor.com/project/kleag/lima)
[![GitHub Action Workflow status](https://github.com/aymara/lima/actions/workflows/build.yml/badge.svg)](https://github.com/aymara/lima/actions)


[![LIMA Python Downloads](https://static.pepy.tech/personalized-badge/aymara?period=total&units=international_system&left_color=black&right_color=brightgreen&left_text=LIMA%20Python%20Downloads)](https://pepy.tech/project/aymara)
