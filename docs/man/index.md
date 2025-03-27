---
layout: default
title: rpm.org - RPM Manual Pages
---

{% assign tools = site.man      | where: 'type', 'tool' %}
{% assign progs = site.man      | where: 'type', 'program' %}
{% assign configs = site.man    | where: 'type', 'config' %}
{% assign plugins = site.man    | where: 'type', 'plugin' %}

# RPM Manual Pages

## System Tools

{% for page in tools -%}
- [{{ page.name }}]({{ page.slug }}) - {{ page.summary }}
{% endfor %}

## User Programs

{% for page in progs -%}
- [{{ page.name }}]({{ page.slug }}) - {{ page.summary }}
{% endfor %}

## Configuration & File Formats

{% for page in configs -%}
- [{{ page.name }}]({{ page.slug }}) - {{ page.summary }}
{% endfor %}

## Plugins

{% for page in plugins -%}
- [{{ page.name }}]({{ page.slug }}) - {{ page.summary }}
{% endfor %}
