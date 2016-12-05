---
layout: default
title: Overview
---

# Recent News

<div class="posts">

<ul>
  {% for post in site.posts limit: 3%}
    <li>
        <span class="recent-news-date">{{ post.date | date_to_string }} »</span>
        <a href="{{ post.url }}" >{{ post.title }}</a>
        <p>{{ post.excerpt }}</p>
    </li>
  {% endfor %}
</ul>

For more news refer to our <a href="{{ site.baseurl }}{{ site.blog_path }}" >blog</a>.

</div>

------------

# Overview

Computer architects have traditionally relied on software simulation to measure dynamic performance metrics (e.g., instructions per cycle) of a proposed computer design. Modern simulation requirements are challenging conventional modeling tools that have served the architectural community in the past. First, the constant emergence of novel heterogeneous devices requires a vertical open-source simulation stack, allowing to expose new devices to the simulated machines. Second, different levels of network integration of the rack interconnect require a detailed and realistic network simulation stack. Third, the ever-growing scale and complexity of these computing systems, along with the detail of software simulators, have place a burden in simulation turnaround times. For instance, detailed software simulators are often six or more orders of magnitude slower than their hardware counterparts. Slow simulation has barred researchers for years from attempting complete benchmarks and input sets of realistic system sizes on detailed simulators.


The QFlex project targets quick, accurate, and flexible simulation of rack-scale computer systems proceeding along four synergistic fronts:

* QEMU is a popular open-source full-system machine emulator that allows functional emulation of unmodified operating systems and applications.
* Flexus is a powerful and flexible simulator framework that allows detailed cycle-accurate simulation that relies heavily on well-defined component interface models.
* NS-3 is a popular, detailed, and flexible network simulation stack.
* SMARTS applies rigorous statistical sampling theory to reduce simulation turnaround by several orders of magnitude, while achieving high accuracy and confidence estimates. 
