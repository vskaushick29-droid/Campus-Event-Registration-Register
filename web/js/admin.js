/**
 * Campus Event Registration Register - Admin Dashboard JavaScript
 * Connects directly to pure C backend APIs with Token Authentication
 */

document.addEventListener('DOMContentLoaded', () => {
    // Session Storage Key for Admin Auth Token
    const TOKEN_KEY = 'campus_admin_token';

    // DOM Elements - Auth & Layout
    const loginSection = document.getElementById('loginSection');
    const dashboardSection = document.getElementById('dashboardSection');
    const userProfile = document.getElementById('userProfile');
    const loginForm = document.getElementById('loginForm');
    const adminUsernameInput = document.getElementById('adminUsername');
    const adminPasswordInput = document.getElementById('adminPassword');
    const loginBtn = document.getElementById('loginBtn');
    const logoutBtn = document.getElementById('logoutBtn');
    const btnText = loginBtn.querySelector('.btn-text');
    const btnSpinner = loginBtn.querySelector('.btn-spinner');

    // Alert Banner
    const adminAlert = document.getElementById('adminAlert');
    const alertIcon = document.getElementById('alertIcon');
    const alertMessage = document.getElementById('alertMessage');
    const closeAlertBtn = document.getElementById('closeAlertBtn');

    // Dashboard Metric Elements
    const statTotal = document.getElementById('statTotal');
    const statRemaining = document.getElementById('statRemaining');
    const eventsGrid = document.getElementById('eventsGrid');

    // Table & Controls Elements
    const searchInput = document.getElementById('searchInput');
    const clearSearchBtn = document.getElementById('clearSearchBtn');
    const eventFilter = document.getElementById('eventFilter');
    const refreshBtn = document.getElementById('refreshBtn');
    const displayedCount = document.getElementById('displayedCount');
    const totalRegisteredCount = document.getElementById('totalRegisteredCount');
    const participantsTableBody = document.getElementById('participantsTableBody');
    const emptyTableState = document.getElementById('emptyTableState');

    // Summary Actions & Modal Elements
    const generateSummaryBtn = document.getElementById('generateSummaryBtn');
    const viewSummaryBtn = document.getElementById('viewSummaryBtn');
    const summaryModal = document.getElementById('summaryModal');
    const closeModalBtn = document.getElementById('closeModalBtn');
    const closeModalFooterBtn = document.getElementById('closeModalFooterBtn');
    const summaryReportContent = document.getElementById('summaryReportContent');
    const copyReportBtn = document.getElementById('copyReportBtn');

    // Debounce Timer for live search
    let searchDebounceTimer = null;

    // ========================================================
    // 1. Initial State & Session Verification
    // ========================================================
    const existingToken = sessionStorage.getItem(TOKEN_KEY);
    if (existingToken) {
        showDashboardView();
        refreshAllDashboardData();
    } else {
        showLoginView();
    }

    function showLoginView() {
        loginSection.classList.remove('hidden');
        dashboardSection.classList.add('hidden');
        userProfile.classList.add('hidden');
        adminUsernameInput.value = '';
        adminPasswordInput.value = '';
        adminUsernameInput.focus();
    }

    function showDashboardView() {
        loginSection.classList.add('hidden');
        dashboardSection.classList.remove('hidden');
        userProfile.classList.remove('hidden');
        hideAlert();
    }

    function getAuthHeaders() {
        const token = sessionStorage.getItem(TOKEN_KEY) || '';
        return {
            'Content-Type': 'application/json',
            'Authorization': `Bearer ${token}`
        };
    }

    // ========================================================
    // 2. Alert Notification Helper
    // ========================================================
    function showAlert(type, message) {
        adminAlert.className = `alert alert-${type}`;
        alertIcon.innerHTML = type === 'success' ? '&#10003;' : '&#9888;';
        alertMessage.textContent = message;
        adminAlert.classList.remove('hidden');
    }

    function hideAlert() {
        adminAlert.classList.add('hidden');
        adminAlert.className = 'alert hidden';
        alertMessage.textContent = '';
    }

    closeAlertBtn.addEventListener('click', hideAlert);

    // ========================================================
    // 3. Admin Authentication (POST /api/admin/login)
    // ========================================================
    loginForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        hideAlert();

        const username = adminUsernameInput.value.trim();
        const password = adminPasswordInput.value.trim();

        if (!username || !password) {
            showAlert('error', 'Please enter both username and password.');
            return;
        }

        // Set Loading State
        loginBtn.disabled = true;
        btnSpinner.classList.remove('hidden');
        btnText.textContent = 'Verifying with C Server...';

        try {
            const response = await fetch('/api/admin/login', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ username, password })
            });

            const data = await response.json();

            if (response.status === 200 && data.status === 'success' && data.token) {
                // Save token in browser session
                sessionStorage.setItem(TOKEN_KEY, data.token);
                showDashboardView();
                refreshAllDashboardData();
                showAlert('success', 'Admin authenticated successfully. Welcome to Campus Event Control Panel.');
            } else {
                showAlert('error', data.message || 'Invalid admin username or password.');
                adminPasswordInput.value = '';
                adminPasswordInput.focus();
            }
        } catch (err) {
            console.error('Login request failed:', err);
            showAlert('error', 'Failed to connect to C backend HTTP server on port 8080.');
        } finally {
            loginBtn.disabled = false;
            btnSpinner.classList.add('hidden');
            btnText.textContent = 'Authenticate & Open Dashboard';
        }
    });

    // Logout
    logoutBtn.addEventListener('click', () => {
        sessionStorage.removeItem(TOKEN_KEY);
        showLoginView();
        showAlert('success', 'You have been signed out of the Admin panel.');
    });

    // ========================================================
    // 4. Load Dashboard Statistics (GET /api/admin/stats)
    // ========================================================
    async function loadDashboardStats() {
        try {
            const response = await fetch('/api/admin/stats', {
                method: 'GET',
                headers: getAuthHeaders()
            });

            if (response.status === 401) {
                handleUnauthorized();
                return;
            }

            const data = await response.json();
            if (data.status === 'success') {
                statTotal.textContent = data.total;
                statRemaining.textContent = data.remaining;

                renderEventCards(data.events || []);
            }
        } catch (err) {
            console.error('Failed to load stats:', err);
        }
    }

    function renderEventCards(events) {
        eventsGrid.innerHTML = '';

        events.forEach(ev => {
            const card = document.createElement('div');
            card.className = 'event-stat-card';
            card.innerHTML = `
                <div>
                    <div class="event-stat-top">
                        <span class="event-stat-name">${escapeHtml(ev.name)}</span>
                        <span class="event-code-pill">${escapeHtml(ev.code)}</span>
                    </div>
                    <div class="event-stat-numbers">
                        <span class="event-count">${ev.count}</span>
                        <span class="event-pct">${ev.percentage.toFixed(1)}%</span>
                    </div>
                </div>
                <div class="progress-bar-bg">
                    <div class="progress-bar-fill" style="width: ${Math.min(ev.percentage, 100)}%;"></div>
                </div>
            `;
            eventsGrid.appendChild(card);
        });
    }

    // ========================================================
    // 5. Load Participants (GET /api/admin/participants)
    // ========================================================
    async function loadParticipants() {
        const query = searchInput.value.trim();
        const evFilter = eventFilter.value;

        let url = '/api/admin/participants?';
        if (query) {
            url += `search=${encodeURIComponent(query)}&`;
        }
        if (evFilter !== '-1') {
            url += `event=${encodeURIComponent(evFilter)}&`;
        }

        try {
            const response = await fetch(url, {
                method: 'GET',
                headers: getAuthHeaders()
            });

            if (response.status === 401) {
                handleUnauthorized();
                return;
            }

            const data = await response.json();
            if (data.status === 'success' && Array.isArray(data.participants)) {
                displayedCount.textContent = data.count;
                totalRegisteredCount.textContent = data.total_registered;

                renderParticipantsTable(data.participants);
            }
        } catch (err) {
            console.error('Failed to load participants:', err);
        }
    }

    function renderParticipantsTable(participants) {
        participantsTableBody.innerHTML = '';

        if (participants.length === 0) {
            emptyTableState.classList.remove('hidden');
            return;
        }

        emptyTableState.classList.add('hidden');

        participants.forEach((p, idx) => {
            const tr = document.createElement('tr');
            tr.innerHTML = `
                <td style="color: var(--text-light); font-weight: 500;">${idx + 1}</td>
                <td><span class="cnf-cell">${escapeHtml(p.confirmation_no || '-')}</span></td>
                <td class="reg-id-cell">${escapeHtml(p.reg_id)}</td>
                <td style="font-weight: 600;">${escapeHtml(p.name)}</td>
                <td>${escapeHtml(p.department)}</td>
                <td>
                    <span class="event-cell-badge badge-event-${p.event_id}">
                        ${escapeHtml(p.event_name)}
                    </span>
                </td>
                <td class="mono">${escapeHtml(p.event_code)}</td>
                <td class="timestamp-cell">${escapeHtml(p.registered_at)}</td>
            `;
            participantsTableBody.appendChild(tr);
        });
    }

    function refreshAllDashboardData() {
        loadDashboardStats();
        loadParticipants();
    }

    function handleUnauthorized() {
        sessionStorage.removeItem(TOKEN_KEY);
        showLoginView();
        showAlert('error', 'Admin session has expired. Please log in again.');
    }

    // ========================================================
    // 6. Search & Filter Interactions
    // ========================================================
    searchInput.addEventListener('input', () => {
        const val = searchInput.value.trim();
        clearSearchBtn.classList.toggle('hidden', val.length === 0);

        // Debounce search query to C backend linear search
        clearTimeout(searchDebounceTimer);
        searchDebounceTimer = setTimeout(() => {
            loadParticipants();
        }, 250);
    });

    clearSearchBtn.addEventListener('click', () => {
        searchInput.value = '';
        clearSearchBtn.classList.add('hidden');
        loadParticipants();
        searchInput.focus();
    });

    eventFilter.addEventListener('change', () => {
        loadParticipants();
    });

    refreshBtn.addEventListener('click', () => {
        refreshAllDashboardData();
        showAlert('success', 'Participant records and statistics refreshed from C database.');
    });

    // ========================================================
    // 7. Event Summary Generation & Viewing
    // ========================================================
    generateSummaryBtn.addEventListener('click', async () => {
        generateSummaryBtn.disabled = true;
        generateSummaryBtn.textContent = 'Generating...';

        try {
            const response = await fetch('/api/admin/summary', {
                method: 'POST',
                headers: getAuthHeaders()
            });

            if (response.status === 401) {
                handleUnauthorized();
                return;
            }

            const data = await response.json();
            if (data.status === 'success') {
                showAlert('success', 'Event summary report successfully generated and written to data/event_summary.txt!');
                // Auto-open summary modal
                openSummaryModal();
            } else {
                showAlert('error', data.message || 'Failed to generate summary report.');
            }
        } catch (err) {
            console.error('Error generating summary:', err);
            showAlert('error', 'Network error while generating summary report.');
        } finally {
            generateSummaryBtn.disabled = false;
            generateSummaryBtn.innerHTML = '&#9881; Generate Summary';
        }
    });

    viewSummaryBtn.addEventListener('click', () => {
        openSummaryModal();
    });

    async function openSummaryModal() {
        summaryModal.classList.remove('hidden');
        summaryReportContent.textContent = 'Loading formatted report from C server...';

        try {
            const response = await fetch('/api/admin/summary', {
                method: 'GET',
                headers: getAuthHeaders()
            });

            if (response.status === 401) {
                handleUnauthorized();
                return;
            }

            const data = await response.json();
            if (data.status === 'success' && data.content) {
                summaryReportContent.textContent = data.content;
            } else {
                summaryReportContent.textContent = 'Report file is empty or not yet generated.';
            }
        } catch (err) {
            console.error('Error fetching summary:', err);
            summaryReportContent.textContent = 'Error: Unable to fetch summary report from C server.';
        }
    }

    function closeSummaryModal() {
        summaryModal.classList.add('hidden');
    }

    closeModalBtn.addEventListener('click', closeSummaryModal);
    closeModalFooterBtn.addEventListener('click', closeSummaryModal);

    summaryModal.addEventListener('click', (e) => {
        if (e.target === summaryModal) {
            closeSummaryModal();
        }
    });

    // Copy Report Text
    copyReportBtn.addEventListener('click', () => {
        const text = summaryReportContent.textContent;
        if (!text) return;

        navigator.clipboard.writeText(text).then(() => {
            const orig = copyReportBtn.textContent;
            copyReportBtn.textContent = 'Copied to Clipboard!';
            setTimeout(() => {
                copyReportBtn.textContent = orig;
            }, 2000);
        }).catch(err => {
            console.error('Copy failed:', err);
        });
    });

    // Helper: HTML Escaping
    function escapeHtml(str) {
        if (!str && str !== 0) return '';
        return String(str)
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&#39;');
    }
});
