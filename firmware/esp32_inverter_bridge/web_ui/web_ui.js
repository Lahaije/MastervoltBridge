// Common JS for Mastervolt Bridge UI

// =============================================================================
// Utility Functions
// =============================================================================

// Get DOM element by ID
const $ = id => document.getElementById(id);

// Unknown value placeholder HTML
const UNKNOWN_HTML = '<span class="unk">-</span>';

// Create a pill (badge) element with text and CSS class
function pill(text, cssClass) {
  return `<span class="pill ${cssClass}">${text}</span>`;
}

// Show a temporary flash message (success or error)
function flash(element, isSuccess, message) {
  element.textContent = message;
  element.className = 'msg ' + (isSuccess ? 'ok' : 'bad');
  setTimeout(() => {
    if (element.textContent === message) {
      element.textContent = '';
    }
  }, 4000);
}

// Check if value is a finite number
function isFiniteNumber(value) {
  return typeof value === 'number' && isFinite(value);
}

// Check if element is currently focused
function isElementFocused(elementId) {
  return document.activeElement === $(elementId);
}

// Format a kWh value for display
function formatKwh(value) {
  return isFiniteNumber(value) ? (value.toFixed(3) + ' kWh') : '-';
}

// Sync toggle/form visibility and populate value based on known state
function syncToggleState(isKnown, unknownRowId, formRowId, statusElement, inputId, value, isBooleanField) {
  $(unknownRowId).hidden = isKnown;
  $(formRowId).hidden = !isKnown;

  if (isKnown) {
    // Value is known; display it
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
    // Value is unknown; show placeholder
    statusElement.textContent = 'unknown';
    statusElement.className = 'unk';
    if (isBooleanField) {
      $(inputId).checked = false;
    } else {
      $(inputId).value = '';
    }
  }
}

// Sync power limit display and form
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

// Sync shadow function display and form
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

// =============================================================================
// API Functions
// =============================================================================

// Perform a JSON GET request
async function apiGet(url) {
  const response = await fetch(url, { cache: 'no-store' });
  if (!response.ok) {
    throw new Error(`HTTP ${response.status}`);
  }
  return response.json();
}

// Perform a JSON POST request
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
    // Response body is not JSON; that's OK
  }

  if (!response.ok) {
    const errorMsg = (data && data.error) || `HTTP ${response.status}`;
    throw new Error(errorMsg);
  }

  return data || {};
}
