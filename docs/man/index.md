---
layout: default
title: rpm.org - RPM Manual Pages
---

{% assign manpages = site.pages | where: 'topic', 'manpage' %}
{% assign tools = manpages      | where: 'type', 'tool' %}
{% assign progs = manpages      | where: 'type', 'program' %}
{% assign configs = manpages    | where: 'type', 'config' %}
{% assign plugins = manpages    | where: 'type', 'plugin' %}

# RPM Manual Pages

## System Tools

{% for page in tools -%}
- [{{ page.title }}]({{ page.slug }}) - {{ page.summary }}
{% endfor %}

## User Programs

{% for page in progs -%}
- [{{ page.title }}]({{ page.slug }}) - {{ page.summary }}
{% endfor %}

## Configuration & File Formats

{% for page in configs -%}
- [{{ page.title }}]({{ page.slug }}) - {{ page.summary }}
{% endfor %}

## Plugins

{% for page in plugins -%}
- [{{ page.title }}]({{ page.slug }}) - {{ page.summary }}
{% endfor %}
