/**
 * Campus Event Registration Portal - Student Interface JavaScript
 * Connects directly to pure C backend via Winsock HTTP Server
 */

document.addEventListener('DOMContentLoaded', () => {
    // DOM Elements
    const form = document.getElementById('registrationForm');
    const regIdInput = document.getElementById('regId');
    const nameInput = document.getElementById('fullName');
    const deptInput = document.getElementById('department');
    const eventSelect = document.getElementById('eventId');
    const submitBtn = document.getElementById('submitBtn');
    const resetBtn = document.getElementById('resetBtn');
    const btnText = submitBtn.querySelector('.btn-text');
    const btnSpinner = submitBtn.querySelector('.btn-spinner');

    const formSection = document.getElementById('formSection');
    const confirmationCard = document.getElementById('confirmationCard');
    const registerAnotherBtn = document.getElementById('registerAnotherBtn');
    const alertBox = document.getElementById('alertBox');
    const alertIcon = document.getElementById('alertIcon');
    const alertMessage = document.getElementById('alertMessage');

    // Field Error Elements
    const regIdError = document.getElementById('regIdError');
    const nameError = document.getElementById('nameError');
    const deptError = document.getElementById('deptError');
    const eventError = document.getElementById('eventError');

    // Confirmation Pass Elements
    const confirmConfirmationNo = document.getElementById('confirmConfirmationNo');
    const confirmPassNumber = document.getElementById('confirmPassNumber');
    const confirmRegId = document.getElementById('confirmRegId');
    const confirmName = document.getElementById('confirmName');
    const confirmDept = document.getElementById('confirmDept');
    const confirmEvent = document.getElementById('confirmEvent');
    const confirmEventCode = document.getElementById('confirmEventCode');
    const confirmTimestamp = document.getElementById('confirmTimestamp');

    // 1. Fetch available events from C backend
    loadAvailableEvents();

    async function loadAvailableEvents() {
        eventSelect.disabled = true;
        eventSelect.innerHTML = '<option value="" disabled selected>-- Loading official events from C backend... --</option>';

        try {
            const response = await fetch('/api/events');
            if (!response.ok) {
                throw new Error(`Server returned HTTP ${response.status}`);
            }

            const data = await response.json();
            if (data.status === 'success' && Array.isArray(data.events)) {
                eventSelect.innerHTML = '<option value="" disabled selected>-- Select an Event --</option>';
                data.events.forEach(ev => {
                    const option = document.createElement('option');
                    option.value = ev.id;
                    option.textContent = `${ev.name} (${ev.code})`;
                    eventSelect.appendChild(option);
                });
                eventSelect.disabled = false;
            } else {
                throw new Error('Invalid events payload received from server');
            }
        } catch (err) {
            console.error('Error fetching events:', err);
            showAlert('error', '⚠️ Unable to connect to C backend server to load events. Please ensure the server is active on port 8080.');
            eventSelect.innerHTML = '<option value="" disabled selected>-- Failed to load events from C server --</option>';
        }
    }

    // 2. Alert Box Display Helper
    function showAlert(type, message) {
        alertBox.className = `alert alert-${type}`;
        if (type === 'success') {
            alertIcon.innerHTML = '&#10003;';
        } else if (type === 'warning') {
            alertIcon.innerHTML = '&#9888;';
        } else {
            alertIcon.innerHTML = '&#9888;';
        }
        alertMessage.textContent = message;
        alertBox.classList.remove('hidden');
        alertBox.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
    }

    function hideAlert() {
        alertBox.classList.add('hidden');
        alertBox.className = 'alert hidden';
        alertMessage.textContent = '';
    }

    // 3. Clear Inline Field Validation Errors
    function clearFieldErrors() {
        regIdError.textContent = '';
        nameError.textContent = '';
        deptError.textContent = '';
        eventError.textContent = '';

        regIdInput.classList.remove('invalid');
        nameInput.classList.remove('invalid');
        deptInput.classList.remove('invalid');
        eventSelect.classList.remove('invalid');
    }

    // 4. Client-side Pre-Validation (Mirrors C Backend Validator)
    function validateForm() {
        clearFieldErrors();
        let isValid = true;

        const regIdVal = regIdInput.value.trim();
        const nameVal = nameInput.value.trim();
        const deptVal = deptInput.value.trim();
        const eventVal = eventSelect.value;

        // Registration ID validation
        if (!regIdVal) {
            regIdError.textContent = 'Registration ID is required.';
            regIdInput.classList.add('invalid');
            isValid = false;
        } else if (regIdVal.length < 3 || regIdVal.length > 30) {
            regIdError.textContent = 'Registration ID must be between 3 and 30 characters.';
            regIdInput.classList.add('invalid');
            isValid = false;
        } else if (!/^[A-Za-z0-9_-]+$/.test(regIdVal)) {
            regIdError.textContent = 'Only letters, numbers, hyphens, and underscores are allowed.';
            regIdInput.classList.add('invalid');
            isValid = false;
        }

        // Full Name validation
        if (!nameVal) {
            nameError.textContent = 'Participant Name is required.';
            nameInput.classList.add('invalid');
            isValid = false;
        } else if (nameVal.length < 2 || nameVal.length > 75) {
            nameError.textContent = 'Name must be between 2 and 75 characters.';
            nameInput.classList.add('invalid');
            isValid = false;
        } else if (!/^[A-Za-z .']+$/.test(nameVal)) {
            nameError.textContent = 'Name must contain letters, spaces, and dots only.';
            nameInput.classList.add('invalid');
            isValid = false;
        }

        // Department validation
        if (!deptVal) {
            deptError.textContent = 'Department is required.';
            deptInput.classList.add('invalid');
            isValid = false;
        } else if (deptVal.length < 2 || deptVal.length > 75) {
            deptError.textContent = 'Department must be between 2 and 75 characters.';
            deptInput.classList.add('invalid');
            isValid = false;
        }

        // Event Selection validation
        if (eventVal === '' || eventVal === null || isNaN(parseInt(eventVal, 10))) {
            eventError.textContent = 'Please select one event from the list.';
            eventSelect.classList.add('invalid');
            isValid = false;
        }

        return isValid;
    }

    // 5. Button State Helper
    function setSubmittingState(isSubmitting) {
        if (isSubmitting) {
            submitBtn.disabled = true;
            btnSpinner.classList.remove('hidden');
            btnText.textContent = 'Registering with C Server...';
        } else {
            submitBtn.disabled = false;
            btnSpinner.classList.add('hidden');
            btnText.textContent = 'Register for Event';
        }
    }

    // 6. Form Submission Event Handler
    form.addEventListener('submit', async (e) => {
        e.preventDefault();
        hideAlert();

        if (!validateForm()) {
            return;
        }

        const payload = {
            reg_id: regIdInput.value.trim(),
            name: nameInput.value.trim(),
            department: deptInput.value.trim(),
            event_id: parseInt(eventSelect.value, 10)
        };

        setSubmittingState(true);

        try {
            const response = await fetch('/api/register', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(payload)
            });

            const data = await response.json();

            if (response.status === 201 && data.status === 'success') {
                // Successful Registration
                const p = data.participant;

                if (confirmConfirmationNo) confirmConfirmationNo.textContent = p.confirmation_no;
                if (confirmPassNumber) confirmPassNumber.textContent = p.confirmation_no;
                confirmRegId.textContent = p.reg_id;
                confirmName.textContent = p.name;
                confirmDept.textContent = p.department;
                confirmEvent.textContent = p.event_name;
                confirmEventCode.textContent = p.event_code;
                confirmTimestamp.textContent = p.registered_at;

                // Switch Views
                formSection.classList.add('hidden');
                confirmationCard.classList.remove('hidden');
                showAlert('success', `Registration confirmed successfully! Your unique Confirmation Number is: ${p.confirmation_no}`);
                confirmationCard.scrollIntoView({ behavior: 'smooth' });

            } else if (response.status === 409) {
                // Duplicate event registration detected by C backend's linear search
                showAlert('warning', `Event Already Registered: ${data.message}`);
                eventError.textContent = data.message;
                eventSelect.classList.add('invalid');
                eventSelect.focus();

            } else if (response.status === 400) {
                // Validation error from C backend
                showAlert('error', `Validation Error: ${data.message}`);

            } else {
                // Other server errors
                showAlert('error', data.message || `Unexpected error (HTTP ${response.status})`);
            }
        } catch (err) {
            console.error('Registration fetch error:', err);
            showAlert('error', 'Cannot connect to C backend HTTP server. Please verify that server.exe is running on port 8080.');
        } finally {
            setSubmittingState(false);
        }
    });

    // 7. Clear Form Button
    resetBtn.addEventListener('click', () => {
        form.reset();
        clearFieldErrors();
        hideAlert();
        regIdInput.focus();
    });

    // 8. Register Another Event Button
    registerAnotherBtn.addEventListener('click', () => {
        // Keep the student's official Registration ID, Name, and Department filled
        // so they can conveniently register for another event!
        const savedRegId = regIdInput.value;
        const savedName = nameInput.value;
        const savedDept = deptInput.value;

        form.reset();
        regIdInput.value = savedRegId;
        nameInput.value = savedName;
        deptInput.value = savedDept;

        clearFieldErrors();
        hideAlert();
        confirmationCard.classList.add('hidden');
        formSection.classList.remove('hidden');
        eventSelect.focus();
        window.scrollTo({ top: 0, behavior: 'smooth' });
    });

    // 9. Copy Confirmation Number
    const copyCnfBtn = document.getElementById('copyCnfBtn');
    const copyCnfText = document.getElementById('copyCnfText');
    const copyCnfIcon = document.getElementById('copyCnfIcon');

    if (copyCnfBtn) {
        copyCnfBtn.addEventListener('click', () => {
            const cnfNo = confirmConfirmationNo ? confirmConfirmationNo.textContent.trim() : '';
            if (cnfNo && cnfNo !== '-') {
                navigator.clipboard.writeText(cnfNo).then(() => {
                    if (copyCnfText) copyCnfText.textContent = 'Copied!';
                    if (copyCnfIcon) copyCnfIcon.innerHTML = '&#10003;';
                    setTimeout(() => {
                        if (copyCnfText) copyCnfText.textContent = 'Copy';
                        if (copyCnfIcon) copyCnfIcon.innerHTML = '&#128203;';
                    }, 2000);
                }).catch(err => {
                    console.error('Failed to copy confirmation number:', err);
                });
            }
        });
    }

    // Real-time error clearance on input
    [regIdInput, nameInput, deptInput, eventSelect].forEach(input => {
        input.addEventListener('input', () => {
            input.classList.remove('invalid');
            const errSpan = document.getElementById(
                input.id === 'regId' ? 'regIdError' :
                    input.id === 'fullName' ? 'nameError' :
                        input.id === 'department' ? 'deptError' : 'eventError'
            );
            if (errSpan) errSpan.textContent = '';
        });
    });
});
