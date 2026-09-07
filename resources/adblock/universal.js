(() => {
  'use strict';
  const KEY = '__googleUiUniversalAdblockV9';
  const existing = window[KEY];
  if (existing && existing.enabled) {
    try { existing.sweep(document); } catch (_) {}
    return;
  }

  const blockedHosts = __HOSTS__;
  const blockedTokens = __TOKENS__;
  const hostLabels = /(^|[._-])(ad(s|x|v|server|service|system|network|tech|track|click|view|unit|slot|tag)?|advert(is(e|ing|ement))?|affiliate|analytics|beacon|bidder|clicktrack|conversion|marketing|metrics|pixel|promo|sponsor(ed)?|telemetry|tracker|tracking)([._-]|$)/i;
  const adWords = /(^|[\s_.:#-])(ad|ads|advert|advertisement|advertising|affiliate|commercial|paid|promoted|promotion|sponsor|sponsored)([\s_.:#-]|$)/i;
  const safeWords = /shadow|adapter|address|admin|adventure|header|loading|road|download|player|playback|video|audio|media/i;
  const isYouTube = /(^|\.)youtube\.com$/i.test(location.hostname) || /(^|\.)youtu\.be$/i.test(location.hostname);

  const explicitAdSelector = [
    '.video-ads', '.ytp-ad-module', '.ytp-ad-overlay-container', '.ytp-ad-player-overlay',
    '.ytp-ad-text-overlay', '.ytp-ad-image-overlay', '.ytp-ad-overlay-slot',
    '.ytp-ad-preview-container', '.ytp-ad-preview-text', '.ytp-ad-simple-ad-badge',
    '.ytp-ad-progress-list', '.ytp-ad-duration-remaining', '.ytp-ad-skip-button-slot',
    'ytd-ad-slot-renderer', 'ytd-display-ad-renderer', 'ytd-promoted-sparkles-web-renderer',
    'ytd-promoted-sparkles-text-search-renderer', 'ytd-promoted-video-renderer',
    'ytd-compact-promoted-video-renderer', 'ytd-in-feed-ad-layout-renderer',
    'ytd-action-companion-ad-renderer', 'ytd-companion-slot-renderer',
    'ytd-video-masthead-ad-advertiser-info-renderer', 'ytd-player-legacy-desktop-watch-ads-renderer',
    'ytm-promoted-sparkles-web-renderer', 'ytm-promoted-video-renderer', 'ytm-companion-ad-renderer'
  ].join(',');

  const protectedMediaSelector = [
    'video', 'audio', '#movie_player', '.html5-video-player', '.html5-video-container',
    'ytd-player', '#player', '#player-container', '#player-container-inner',
    '.ytp-player-content', '.ytp-chrome-bottom', '.ytp-chrome-top', '.ytp-progress-bar-container'
  ].join(',');

  const state = {
    enabled: true,
    observer: null,
    timer: 0,
    originals: {},
    style: null,
    sweep: null,
    cleanup: null
  };
  window[KEY] = state;

  function hostnameMatches(host, rule) {
    host = String(host || '').toLowerCase().replace(/^www\./, '');
    rule = String(rule || '').toLowerCase();
    if (!host || !rule) return false;
    if (rule.includes('/')) return false;
    return host === rule || host.endsWith('.' + rule);
  }

  function shouldBlock(raw) {
    if (!state.enabled || raw == null) return false;
    let text = String(raw).trim();
    if (!text || /^(data|blob|javascript|about):/i.test(text)) return false;
    let url;
    try { url = new URL(text, document.baseURI || location.href); }
    catch (_) { return false; }
    const host = String(url.hostname || '').toLowerCase().replace(/^www\./, '');
    const full = (host + url.pathname + url.search).toLowerCase();
    for (const rule of blockedHosts) {
      if (rule.includes('/')) {
        if (full.includes(rule)) return true;
      } else if (hostnameMatches(host, rule)) {
        return true;
      }
    }
    if (hostLabels.test(host)) return true;
    for (const token of blockedTokens) {
      if (full.includes(token)) return true;
    }
    return false;
  }

  function neutralResponse() {
    try { return Promise.resolve(new Response('', { status: 204, statusText: 'Blocked' })); }
    catch (_) { return Promise.reject(new Error('Blocked by GoogleUI')); }
  }

  function isYouTubePlayerResponse(raw) {
    if (!isYouTube || raw == null) return false;
    try {
      const url = new URL(String(raw), location.href);
      return /\/youtubei\/v1\/(player|next|browse)(?:\?|$)/i.test(url.pathname + url.search);
    } catch (_) {
      return false;
    }
  }

  function sanitizeYouTubeText(text) {
    if (!isYouTube || typeof text !== 'string' || !text) return text;
    try {
      const parse = state.originals.jsonParse || JSON.parse;
      const value = parse.call(JSON, text);
      return JSON.stringify(stripYouTubeAds(value));
    } catch (_) {
      return text;
    }
  }

  function isExplicitAdNode(element) {
    if (!element || element.nodeType !== 1 || !element.matches) return false;
    try { return element.matches(explicitAdSelector); } catch (_) { return false; }
  }

  function isProtectedMediaNode(element) {
    if (!element || element.nodeType !== 1) return false;
    if (isExplicitAdNode(element)) return false;
    try {
      if (element.matches && element.matches(protectedMediaSelector)) return true;
      const protectedAncestor = element.closest && element.closest(protectedMediaSelector);
      if (!protectedAncestor) return false;
      // Ad overlays and ad renderer children inside a protected player may still be removed.
      return !isExplicitAdNode(element);
    } catch (_) {
      return false;
    }
  }

  function stripYouTubeAds(value, depth = 0, seen = null) {
    if (!isYouTube || value == null || typeof value !== 'object' || depth > 12) return value;
    if (!seen) seen = new WeakSet();
    if (seen.has(value)) return value;
    seen.add(value);
    const exactKeys = new Set([
      'adplacements', 'playerads', 'adslots', 'adbreakheartbeatparams', 'adparams',
      'adsignalsinfo', 'adplacementconfig', 'adplacementrenderer', 'adlayoutmetadata',
      'adbreakservicerenderer', 'instreamvideoadrenderer', 'linearadsequencerenderer',
      'displayadrenderer', 'adslotrenderer', 'infeedadlayoutrenderer',
      'mastheadadrenderer', 'companionadrenderer', 'actioncompanionadrenderer',
      'promotedvideorenderer', 'compactpromotedvideorenderer',
      'promotedsparkleswebrenderer', 'promotedsparklestextsearchrenderer',
      'playerlegacydesktopwatchadsrenderer'
    ]);
    if (Array.isArray(value)) {
      for (let i = value.length - 1; i >= 0; --i) {
        const item = value[i];
        if (item && typeof item === 'object') {
          const keys = Object.keys(item).map(k => String(k).toLowerCase());
          if (keys.some(k => exactKeys.has(k) || (k.endsWith('adrenderer') && k !== 'badgeRenderer'.toLowerCase()) || (k.includes('promoted') && k.endsWith('renderer')))) {
            value.splice(i, 1);
            continue;
          }
          stripYouTubeAds(item, depth + 1, seen);
        }
      }
      return value;
    }
    for (const key of Object.keys(value)) {
      const lower = String(key).toLowerCase();
      if (exactKeys.has(lower) || (lower.endsWith('adrenderer') && lower !== 'badgerenderer') || (lower.includes('promoted') && lower.endsWith('renderer'))) {
        try { delete value[key]; } catch (_) {}
        continue;
      }
      stripYouTubeAds(value[key], depth + 1, seen);
    }
    return value;
  }

  if (typeof window.fetch === 'function') {
    state.originals.fetch = window.fetch;
    window.fetch = function(input, init) {
      const target = input && typeof input === 'object' && 'url' in input ? input.url : input;
      if (shouldBlock(target)) return neutralResponse();
      const result = state.originals.fetch.apply(this, arguments);
      if (!isYouTubePlayerResponse(target)) return result;
      return result.then(response => response.clone().text().then(text => {
        const sanitized = sanitizeYouTubeText(text);
        if (sanitized === text) return response;
        return new Response(sanitized, {
          status: response.status,
          statusText: response.statusText,
          headers: response.headers
        });
      }).catch(() => response));
    };
  }

  if (isYouTube && window.Response && Response.prototype && typeof Response.prototype.json === 'function') {
    state.originals.responseJson = Response.prototype.json;
    Response.prototype.json = function() {
      return state.originals.responseJson.apply(this, arguments).then(value => stripYouTubeAds(value));
    };
  }

  if (isYouTube && typeof JSON.parse === 'function') {
    state.originals.jsonParse = JSON.parse;
    JSON.parse = function() {
      return stripYouTubeAds(state.originals.jsonParse.apply(JSON, arguments));
    };
  }

  if (window.XMLHttpRequest && XMLHttpRequest.prototype) {
    const proto = XMLHttpRequest.prototype;
    state.originals.xhrOpen = proto.open;
    state.originals.xhrSend = proto.send;
    proto.open = function(method, url) {
      this.__googleuiBlocked = shouldBlock(url);
      this.__googleuiYouTubeResponse = isYouTubePlayerResponse(url);
      return state.originals.xhrOpen.apply(this, arguments);
    };
    proto.send = function() {
      if (this.__googleuiBlocked) {
        try { this.abort(); } catch (_) {}
        return;
      }
      if (this.__googleuiYouTubeResponse) {
        this.addEventListener('readystatechange', function() {
          if (this.readyState !== 4) return;
          try {
            const sanitized = sanitizeYouTubeText(this.responseText);
            if (sanitized !== this.responseText) {
              Object.defineProperty(this, 'responseText', { configurable: true, value: sanitized });
              if (!this.responseType || this.responseType === 'text') {
                Object.defineProperty(this, 'response', { configurable: true, value: sanitized });
              }
            }
          } catch (_) {}
        });
      }
      return state.originals.xhrSend.apply(this, arguments);
    };
  }

  if (navigator.sendBeacon) {
    state.originals.sendBeacon = navigator.sendBeacon.bind(navigator);
    navigator.sendBeacon = function(url, data) {
      return shouldBlock(url) ? true : state.originals.sendBeacon(url, data);
    };
  }

  for (const ctorName of ['WebSocket', 'EventSource']) {
    const Original = window[ctorName];
    if (typeof Original !== 'function') continue;
    state.originals[ctorName] = Original;
    const Wrapped = function(url, protocols) {
      if (shouldBlock(url)) throw new DOMException('Blocked by GoogleUI', 'SecurityError');
      return protocols === undefined ? new Original(url) : new Original(url, protocols);
    };
    Wrapped.prototype = Original.prototype;
    try { Object.setPrototypeOf(Wrapped, Original); } catch (_) {}
    window[ctorName] = Wrapped;
  }

  state.originals.open = window.open;
  window.open = function(url) {
    if (!url || shouldBlock(url)) return null;
    return state.originals.open ? state.originals.open.apply(window, arguments) : null;
  };

  if (Element.prototype.setAttribute) {
    state.originals.setAttribute = Element.prototype.setAttribute;
    Element.prototype.setAttribute = function(name, value) {
      const lower = String(name || '').toLowerCase();
      if ((lower === 'src' || lower === 'href' || lower === 'data-src') && shouldBlock(value)) {
        try { this.dataset.googleBlocked = '1'; } catch (_) {}
        return;
      }
      return state.originals.setAttribute.apply(this, arguments);
    };
  }

  const selectors = [
    'iframe[src*="doubleclick"]', 'iframe[src*="googlesyndication"]',
    'iframe[src*="googleadservices"]', 'iframe[src*="amazon-adsystem"]',
    'script[src*="doubleclick"]', 'script[src*="googlesyndication"]',
    'script[src*="googleadservices"]', 'script[src*="adservice"]',
    'img[src*="doubleclick"]', 'img[src*="googlesyndication"]',
    '[data-ad]', '[data-ads]', '[data-ad-id]', '[data-ad-slot]', '[data-ad-unit]',
    '[data-adunit]', '[data-advertisement]', '[data-sponsored]',
    '[aria-label="Advertisement"]', '[aria-label="Ads"]',
    '.adsbygoogle', '#google_ads_iframe', '[id^="google_ads_"]',
    ...explicitAdSelector.split(','),
    '[data-pagelet*="Ad"]', '[data-testid="placementTracking"]',
    'div[role="complementary"] iframe[src*="ads"]'
  ];

  function looksLikeAd(node) {
    if (!node || node.nodeType !== 1) return false;
    const element = node;
    if (isExplicitAdNode(element)) return true;
    if (isProtectedMediaNode(element)) return false;
    const tag = String(element.tagName || '').toLowerCase();
    const source = element.getAttribute &&
      (element.getAttribute('src') || element.getAttribute('href') || element.getAttribute('data-src'));
    if (source && shouldBlock(source)) return true;
    if (element.dataset && element.dataset.googleBlocked === '1') return true;

    const identity = [element.id, element.className,
      element.getAttribute && element.getAttribute('aria-label'),
      element.getAttribute && element.getAttribute('data-testid')]
      .filter(Boolean).join(' ').slice(0, 512);
    if (identity && !safeWords.test(identity) && adWords.test(identity)) return true;

    if (tag === 'iframe' || tag === 'script' || tag === 'img' || tag === 'link') {
      return !!(source && shouldBlock(source));
    }
    return false;
  }

  function removeSponsoredFeeds(root) {
    const candidates = root.querySelectorAll ? root.querySelectorAll(
      'article, [role="article"], ytd-rich-item-renderer, ytd-video-renderer, div[data-pagelet^="FeedUnit_"]'
    ) : [];
    for (const item of candidates) {
      if (isProtectedMediaNode(item)) continue;
      const text = String(item.innerText || '').slice(0, 600);
      if (/\b(Sponsored|Promoted|Paid partnership|Suggested for you)\b/i.test(text)) {
        item.remove();
      }
    }
  }

  function youtubeCleanup(root) {
    try {
      if (window.ytInitialPlayerResponse) stripYouTubeAds(window.ytInitialPlayerResponse);
      if (window.ytInitialData) stripYouTubeAds(window.ytInitialData);
      root.querySelectorAll(explicitAdSelector).forEach(node => node.remove());

      const skipSelectors = [
        '.ytp-ad-skip-button', '.ytp-ad-skip-button-modern', '.ytp-skip-ad-button',
        'button.ytp-ad-skip-button-modern', '.ytp-ad-skip-button-container button',
        '.ytp-ad-skip-button-slot button', 'button[aria-label*="Skip"]'
      ].join(',');
      root.querySelectorAll(skipSelectors).forEach(skip => {
        try { skip.click(); } catch (_) {}
      });
      for (const button of root.querySelectorAll('button')) {
        const label = String(button.getAttribute('aria-label') || button.textContent || '').trim();
        if (/^(skip|skip ad|skip ads|skip video)/i.test(label)) {
          try { button.click(); } catch (_) {}
        }
      }

      const player = root.querySelector('#movie_player,.html5-video-player');
      const video = player ? player.querySelector('video') : root.querySelector('video');
      const adIndicator = root.querySelector([
        '.ytp-ad-preview-container', '.ytp-ad-preview-text', '.ytp-ad-simple-ad-badge',
        '.ytp-ad-duration-remaining', '.ytp-ad-skip-button-slot', '.video-ads'
      ].join(','));
      const adShowing = !!((player && (player.classList.contains('ad-showing') ||
        player.classList.contains('ad-interrupting'))) || adIndicator ||
        root.querySelector('ytd-player.ad-showing,ytm-player.ad-showing'));
      if (video && adShowing) {
        if (!video.__googleuiAdPlaybackState) {
          video.__googleuiAdPlaybackState = {
            muted: !!video.muted,
            playbackRate: Number(video.playbackRate) || 1,
            volume: Number(video.volume)
          };
        }
        try { video.muted = true; } catch (_) {}
        try { video.volume = 0; } catch (_) {}
        try { video.playbackRate = 16; } catch (_) {}
        try {
          if (player && typeof player.seekTo === 'function' && Number.isFinite(video.duration)) {
            player.seekTo(video.duration, true);
          }
        } catch (_) {}
        try {
          if (Number.isFinite(video.duration) && video.duration > 0.25) {
            video.currentTime = Math.max(0, video.duration - 0.05);
          }
        } catch (_) {}
        try {
          const promise = video.play();
          if (promise && typeof promise.catch === 'function') promise.catch(() => {});
        } catch (_) {}
      } else if (video && video.__googleuiAdPlaybackState) {
        const old = video.__googleuiAdPlaybackState;
        try { video.muted = old.muted; } catch (_) {}
        try { if (Number.isFinite(old.volume)) video.volume = old.volume; } catch (_) {}
        try { video.playbackRate = old.playbackRate; } catch (_) {}
        try { delete video.__googleuiAdPlaybackState; } catch (_) { video.__googleuiAdPlaybackState = null; }
      }
    } catch (_) {}
  }

  function sweep(root) {
    if (!state.enabled || !root) return;
    try {
      for (const selector of selectors) {
        root.querySelectorAll(selector).forEach(node => {
          if (!isProtectedMediaNode(node) || isExplicitAdNode(node)) node.remove();
        });
      }
      root.querySelectorAll('iframe,script,img,source,link,[id],[class],[data-ad],[data-sponsored]')
        .forEach(node => { if (looksLikeAd(node)) node.remove(); });
      removeSponsoredFeeds(root);
      if (isYouTube) youtubeCleanup(root);
    } catch (_) {}
  }
  state.sweep = sweep;

  const style = document.createElement('style');
  style.id = 'google-universal-adblock-style';
  style.textContent = selectors.join(',') + '{display:none!important;visibility:hidden!important;max-height:0!important;min-height:0!important;height:0!important;overflow:hidden!important;pointer-events:none!important}';
  (document.head || document.documentElement).appendChild(style);
  state.style = style;

  state.observer = new MutationObserver(records => {
    for (const record of records) {
      for (const node of record.addedNodes || []) {
        if (node && node.nodeType === 1) {
          if (looksLikeAd(node)) node.remove();
          else sweep(node);
        }
      }
      if (isYouTube && record.target && record.target.nodeType === 1) {
        try { youtubeCleanup(record.target.ownerDocument || document); } catch (_) {}
      }
    }
  });
  state.observer.observe(document.documentElement || document, {
    childList: true,
    subtree: true,
    attributes: true,
    attributeFilter: ['src','href','class','id','data-ad','data-sponsored']
  });
  state.timer = setInterval(() => sweep(document), isYouTube ? 100 : 1250);
  state.cleanup = function() {
    state.enabled = false;
    try { state.observer && state.observer.disconnect(); } catch (_) {}
    try { state.timer && clearInterval(state.timer); } catch (_) {}
    try { state.style && state.style.remove(); } catch (_) {}
    if (isYouTube) {
      try { if (state.originals.responseJson) Response.prototype.json = state.originals.responseJson; } catch (_) {}
      try { if (state.originals.jsonParse) JSON.parse = state.originals.jsonParse; } catch (_) {}
    }
    try { if (state.originals.fetch) window.fetch = state.originals.fetch; } catch (_) {}
    try {
      if (state.originals.xhrOpen) XMLHttpRequest.prototype.open = state.originals.xhrOpen;
      if (state.originals.xhrSend) XMLHttpRequest.prototype.send = state.originals.xhrSend;
    } catch (_) {}
  };
  sweep(document);
})();
