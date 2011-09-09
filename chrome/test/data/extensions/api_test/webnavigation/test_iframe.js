// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function runTests() {
  var getURL = chrome.extension.getURL;
  chrome.tabs.create({"url": "about:blank"}, function(tab) {
    var tabId = tab.id;

    chrome.test.runTests([
      // Navigates to a.html which includes b.html as an iframe. b.html
      // redirects to c.html.
      function iframe() {
        expect([
          [ "onBeforeNavigate",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/a.html') }],
          [ "onCommitted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              transitionQualifiers: [],
              transitionType: "link",
              url: getURL('iframe/a.html') }],
          [ "onBeforeNavigate",
            { frameId: 1,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/b.html') }],
          [ "onDOMContentLoaded",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/a.html') }],
          [ "onCommitted",
            { frameId: 1,
              tabId: 0,
              timeStamp: 0,
              transitionQualifiers: [],
              transitionType: "auto_subframe",
              url: getURL('iframe/b.html') }],
          [ "onDOMContentLoaded",
            { frameId: 1,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/b.html') }],
          [ "onCompleted",
            { frameId: 1,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/b.html') }],
          [ "onCompleted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/a.html') }],
          [ "onBeforeNavigate",
            { frameId: 1,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/c.html') }],
          [ "onCommitted",
            { frameId: 1,
              tabId: 0,
              timeStamp: 0,
              transitionQualifiers: [],
              transitionType: "manual_subframe",
              url: getURL('iframe/c.html') }],
          [ "onDOMContentLoaded",
            { frameId: 1,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/c.html') }],
          [ "onCompleted",
            { frameId: 1,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/c.html') }]]);
        chrome.tabs.update(tabId, { url: getURL('iframe/a.html') });
      },

      // Navigates to d.html which includes e.html and f.html as iframes. To be
      // able to predict which iframe has which id, the iframe for f.html is
      // created by javascript. f.html then navigates to g.html.
      function iframeMultiple() {
        expect([
          [ "onBeforeNavigate",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/d.html') }],
          [ "onCommitted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              transitionQualifiers: [],
              transitionType: "link",
              url: getURL('iframe/d.html') }],
          [ "onBeforeNavigate",
            { frameId: 1,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/e.html') }],
          [ "onDOMContentLoaded",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/d.html') }],
          [ "onCommitted",
            { frameId: 1,
              tabId: 0,
              timeStamp: 0,
              transitionQualifiers: [],
              transitionType: "auto_subframe",
              url: getURL('iframe/e.html') }],
          [ "onDOMContentLoaded",
            { frameId: 1,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/e.html') }],
          [ "onBeforeNavigate",
            { frameId: 2,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/f.html') }],
          [ "onCompleted",
            { frameId: 1,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/e.html') }],
          [ "onCommitted",
            { frameId: 2,
              tabId: 0,
              timeStamp: 0,
              transitionQualifiers: [],
              transitionType: "auto_subframe",
              url: getURL('iframe/f.html') }],
          [ "onDOMContentLoaded",
            { frameId: 2,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/f.html') }],
          [ "onCompleted",
            { frameId: 2,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/f.html') }],
          [ "onCompleted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/d.html') }],
          [ "onBeforeNavigate",
            { frameId: 2,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/g.html') }],
          [ "onCommitted",
            { frameId: 2,
              tabId: 0,
              timeStamp: 0,
              transitionQualifiers: [],
              transitionType: "manual_subframe",
              url: getURL('iframe/g.html') }],
          [ "onDOMContentLoaded",
            { frameId: 2,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/g.html') }],
          [ "onCompleted",
            { frameId: 2,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/g.html') }]]);
        chrome.tabs.update(tabId, { url: getURL('iframe/d.html') });
      },

      // Navigates to h.html which includes i.html that triggers a navigation
      // on the main frame.
      function iframeNavigate() {
        expect([
          [ "onBeforeNavigate",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/h.html') }],
          [ "onCommitted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              transitionQualifiers: [],
              transitionType: "link",
              url: getURL('iframe/h.html') }],
          [ "onBeforeNavigate",
            { frameId: 1,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/i.html') }],
          [ "onDOMContentLoaded",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/h.html') }],
          [ "onCommitted",
            { frameId: 1,
              tabId: 0,
              timeStamp: 0,
              transitionQualifiers: [],
              transitionType: "auto_subframe",
              url: getURL('iframe/i.html') }],
          [ "onDOMContentLoaded",
            { frameId: 1,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/i.html') }],
          [ "onCompleted",
            { frameId: 1,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/i.html') }],
          [ "onCompleted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/h.html') }],
          [ "onBeforeNavigate",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/c.html') }],
          [ "onCommitted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              transitionQualifiers: [],
              transitionType: "link",
              url: getURL('iframe/c.html') }],
          [ "onDOMContentLoaded",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/c.html') }],
          [ "onCompleted",
            { frameId: 0,
              tabId: 0,
              timeStamp: 0,
              url: getURL('iframe/c.html') }]]);
        chrome.tabs.update(tabId, { url: getURL('iframe/h.html') });
      },
    ]);
  });
}
