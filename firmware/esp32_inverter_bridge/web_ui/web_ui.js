// Shared UI helpers.

const $ = id => document.getElementById(id);

const UNKNOWN_HTML = '<span class="unk">-</span>';

function pill(text, cssClass) {
  return `<span class="pill ${cssClass}">${text}</span>`;
}

function flash(element, isSuccess, message) {
  element.textContent = message;
  element.className = 'msg ' + (isSuccess ? 'ok' : 'bad');
  setTimeout(() => {
    if (element.textContent === message) {
      element.textContent = '';
    }
  }, 4000);
}

function isFiniteNumber(value) {
  return typeof value === 'number' && isFinite(value);
}

function isElementFocused(elementId) {
  return document.activeElement === $(elementId);
}

function formatKwh(value) {
  return isFiniteNumber(value) ? (value.toFixed(3) + ' kWh') : '-';
}

function syncToggleState(isKnown, unknownRowId, formRowId, statusElement, inputId, value, isBooleanField) {
  $(unknownRowId).hidden = isKnown;
  $(formRowId).hidden = !isKnown;

  if (isKnown) {
    statusElement.textContent = '';
    statusElement.className = '';
    if (!isElementFocused(inputId)) {
      if (isBooleanField) {
        $(inputId).checked = value;
      } else {
        $(inputId).value = value;
      }
    }
  } else {
    statusElement.textContent = 'unknown';
    statusElement.className = 'unk';
    if (isBooleanField) {
      $(inputId).checked = false;
    } else {
      $(inputId).value = '';
    }
  }
}

function syncPowerDisplay(info) {
  syncToggleState(
    isFiniteNumber(info.power_limit_watts),
    'rowPowerUnknown',
    'rowPowerForm',
    $('curPower'),
    'iPower',
    info.power_limit_watts,
    false
  );
}

function syncShadowDisplay(info) {
  syncToggleState(
    typeof info.shadow_enabled === 'boolean',
    'rowShadowUnknown',
    'rowShadowForm',
    $('curShadow'),
    'iShadow',
    info.shadow_enabled,
    true
  );
}

async function apiGet(url) {
  const response = await fetch(url, { cache: 'no-store' });
  if (!response.ok) {
    throw new Error(`HTTP ${response.status}`);
  }
  return response.json();
}

async function apiPost(url, body) {
  const response = await fetch(url, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  });

  let data = null;
  try {
    data = await response.json();
  } catch (err) {
  }

  if (!response.ok) {
    const errorMsg = (data && data.error) || `HTTP ${response.status}`;
    throw new Error(errorMsg);
  }

  return data || {};
}
