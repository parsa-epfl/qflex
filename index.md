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

For more news refer to our <a href="{{ site.url }}{{ site.blog_path }}" >blog</a>.

</div>

------------

# Overview

Computer architects have traditionally relied on software simulation to measure the performance metrics (e.g., instructions per cycle) of a proposed design. However, modern simulation requirements are challenging the conventional modeling tools that have traditionally served the architecture community. First, the constant emergence of new devices calls for a vertical open-source simulation stack, allowing to expose the aforesaid devices to the simulated machines. Second, multi-node computer systems have become the norm, and hence the different levels of network integration requires a detailed and realistic network simulation stack. Third, the ever-growing scale and complexity of computer systems, along with the detail of software simulation, have placed a burden in simulation turnaround times. For instance, detailed software simulators are often six or more orders of magnitude slower than their hardware counterparts. Slow simulation has barred researchers from attempting complete benchmarks and input sets of realistic system sizes.

The QFlex project targets quick, accurate, and flexible simulation of multi-node computer systems proceeding along four synergistic fronts:

* QEMU is a popular open-source full-system machine emulator that allows functional emulation of unmodified operating systems and applications.
* Flexus is a powerful and flexible simulator framework that allows detailed cycle-accurate simulation that relies heavily on well-defined component interface models.
* NS-3 is a popular, detailed, and flexible network simulation stack.
* SMARTS applies rigorous statistical sampling theory to reduce the simulation turnaround time by several orders of magnitude, while achieving high accuracy and confidence estimates. 
