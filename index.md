---
layout: default
title: Overview
---

# Recent News

<div class="posts">

<ul>
  {% for post in site.posts limit: 3%}
    <li>
          <h4 class="post-title">
            <span class="recent-news-date">{{ post.date | date_to_string }} &raquo;</span>
            <a href="{{ site.url }}{{ post.url }}">{{ post.title }}</a>
          </h4>
          <p>{{ post.excerpt }}
          
          {% capture content_words %}
          {{ post.content | number_of_words }}
          {% endcapture %}
          {% capture excerpt_words %}
          {{ post.excerpt | number_of_words }}
          {% endcapture %}
          {% if excerpt_words != content_words %}
            <a href="{{ site.url }}{{ post.url }}">Read more...</a>
          {% endif %}
          </p>
    </li>
    <hr/>
  {% endfor %}
</ul>

For more news refer to our <a href="{{ site.url }}{{ site.blog_path }}" >blog</a>.

</div>

------------

# Overview

Computer architects have traditionally relied on software simulation to measure the performance metrics (e.g., instructions per cycle) of a proposed design. However, modern simulation requirements are challenging the conventional modeling tools that have traditionally served the architecture community. First, the constant emergence of new devices calls for a vertical open-source simulation stack, allowing to expose the these devices to interface with existing microarchitectural simulators. Second, multi-node computer systems have become the norm. Hence, architecture simulators must include a much more detailed network model. Third, the increasing complexity of computer systems across all layers of the stack, makes it very difficult to ensure fast simulation turnaround times. For instance, detailed software simulators are often six or more orders of magnitude slower than their hardware counterparts. Slow simulation has barred researchers from attempting complete benchmarks and input sets of realistic system sizes.

The QFlex project targets quick, accurate, and flexible simulation of multi-node computer systems proceeding along four fronts:

* QEMU is a popular open-source full-system machine emulator that allows functional emulation of unmodified operating systems and applications.
* Flexus is a powerful and flexible simulator framework that allows detailed cycle-accurate simulation.
* NS-3 is a popular, detailed, and flexible network simulation stack.
* SMARTS applies rigorous statistical sampling theory to reduce the simulation turnaround time by several orders of magnitude, while achieving high accuracy and confidence estimates. 
