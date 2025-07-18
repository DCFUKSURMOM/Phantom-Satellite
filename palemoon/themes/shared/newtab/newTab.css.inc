/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

:root {
  -moz-appearance: none;
  font-size: 75%;
  background-color: rgba(000,000,000,255);
}

body {
  background: #000;
}

/* SCROLLBOX */
#newtab-scrollbox:not([page-disabled]) {
  background-color: #eee;
  background-attachment: fixed;
}

/* UNDO */
#newtab-undo-container {
  padding: 4px 3px;
  border: 1px solid;
  border-color: rgba(8,22,37,.12) rgba(8,22,37,.14) rgba(8,22,37,.16);
  background-color: rgba(255,255,255,.4);
  color: #525e69;
}

#newtab-undo-label {
  margin-top: 0;
  margin-bottom: 0;
}


.newtab-undo-button:hover {
  text-decoration: underline;
}

.newtab-undo-button:-moz-focusring {
  outline: 1px dotted;
}

#newtab-undo-close-button {
  -moz-appearance: none;
  padding: 0;
  border: none;
  width: 16px;
  height: 16px;
  float: right;
  right: 0;
}

/* TOGGLE */
#newtab-toggle {
  width: 16px;
  height: 16px;
  padding: 0;
  border: none;
  background: -216px 0 transparent url(chrome://browser/skin/newtab/controls.png);
}

#newtab-toggle[page-disabled] {
  background-position: -232px 0;
}

/* ROWS */
.newtab-row {
  margin-bottom: 20px;
}

.newtab-row:last-child {
  margin-bottom: 0;
}

/* CELLS */
.newtab-cell {
  -moz-margin-end: 20px;
  background-color: rgba(0,0,0,1);
  border: 1px solid;
  border-color: rgba(0,255,255,1) rgba(0,255,255,1) rgba(0,255,255,1);
  border-radius: 3.5px;
  transition: border-color 100ms ease-out;
}

.newtab-cell:empty {
  border: 1px solid;
  border-color: rgba(0,255,255,1) rgba(0,255,255,1) rgba(0,255,255,1);
}

.newtab-cell:last-child {
  -moz-margin-end: 0;
}

.newtab-cell:hover:not(:empty):not([dragged]) {
  border-color: rgba(8,16,37,.25) rgba(8,16,37,.27) rgba(8,16,37,.3);
}

/* SITES */
.newtab-site {
  text-decoration: none;
  transition-property: top, left, opacity, box-shadow, background-color;
  border-radius: 2.5px;
  overflow: hidden;
}

.newtab-site:hover,
.newtab-site[dragged] {
  box-shadow: 0 3px 6px 0px rgba(75,80,89,.6);
}

.newtab-site[dragged] {
  transition-property: box-shadow, background-color;
  background-color: rgb(0,0,0);
}

/* THUMBNAILS */
.newtab-thumbnail {
  background-origin: padding-box;
  background-clip: padding-box;
  background-repeat: no-repeat;
  background-size: cover;
}

/* TITLES */
.newtab-title {
  padding: 0 8px 1px;
  background: linear-gradient(to bottom, rgba(0, 30, 30, 1), rgba(0, 30, 30, 1));
  color: #00ffff;
  line-height: 24px;
}

.newtab-site[pinned] .newtab-title {
  padding-inline-start: 16px;
}

.newtab-site[pinned] .newtab-title::before {
  background-image: url(chrome://browser/skin/newtab/pinned.png);
  content: "";
  left: 2px;
  top: 2px;
  position: absolute;
  width: 12px;
  height: 20px;
}

.newtab-site[pinned] .newtab-title:dir(rtl)::before {
  left: auto;
  right: 2px;
}

/* CONTROLS */
.newtab-control {
  background-color: transparent;
  background-size: 24px;
  border: none;
  height: 24px;
  width: 24px;
  top: 4px;
  background: transparent url(chrome://browser/skin/newtab/controls.png);
}

.newtab-control-pin:dir(ltr),
.newtab-control-block:dir(rtl) {
  left: 4px;
}

.newtab-control-block:dir(ltr),
.newtab-control-pin:dir(rtl) {
  right: 4px;
}

.newtab-control-pin:hover {
  background-position: -24px 0;
}

.newtab-control-pin:active {
  background-position: -48px 0;
}

.newtab-site[pinned] .newtab-control-pin {
  background-position: -72px 0;
}

.newtab-site[pinned] .newtab-control-pin:hover {
  background-position: -96px 0;
}

.newtab-site[pinned] .newtab-control-pin:active {
  background-position: -120px 0;
}

.newtab-control-block {
  background-position: -144px 0;
}

.newtab-control-block:hover {
  background-position: -168px 0;
}

.newtab-control-block:active {
  background-position: -192px 0;
}

/* SEARCH */
#searchEngineLogo {
  /* remove the default DDG image - it looks really bad on our dark b/g */
  opacity: 0;
}

#searchText {
  -moz-box-flex: 1;
  color: #00ffff;
  padding: 6px 8px;
  background-color: rgb(0, 0, 0);
  border: 1px solid;
  border-color: rgb(0, 255, 255);
  border-radius: 5px 0 0 5px;
  box-shadow: none;
}

#searchText:-moz-dir(rtl) {
  border-radius: 0 5px 5px 0;
}

#searchText:focus,
#searchText[autofocus] {
  border-color: rgb(0, 255, 255);
}

#searchSubmit {
  margin-inline-start: -1px;
  padding: 0 9px;
  color: #fff;
  background-color: rgb(0, 0, 0);
  border: 1px solid;
  border-color: rgb(0, 255,255);
  border-radius: 0 5px 5px 0;
  cursor: pointer;
  transition-property: background-color, border-color, box-shadow;
  transition-duration: 150ms;
  box-shadow: none;
}

#searchSubmit:-moz-dir(rtl) {
  border-radius: 5px 0 0 5px;
}

#searchText:focus + #searchSubmit,
#searchText + #searchSubmit:hover,
#searchText[autofocus] + #searchSubmit {
  border-color: rgb(0, 255, 255);
  background-color: rgb(0, 0, 0);
}

#searchText + #searchSubmit:hover:active {
  border-color: rgb(0,255, 255);
  background-color: rgb(0, 0, 0);
}
