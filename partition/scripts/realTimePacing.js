document.addEventListener("DOMContentLoaded", function() {
    // Fetch the initial lap and lapTime values when the page is loaded
    fetchLapValue();
    fetchLapTime();
    fetchStatus();
});

// Function to fetch the current lap value from the server
function fetchLapValue() {
    fetch('/getLap')
        .then(response => response.text())
        .then(data => {
            document.getElementById("lapValue").innerText = data; // Update lap value
        })
        .catch(error => console.error('Error fetching lap value:', error));
}

// Function to fetch the current lap time from the server
function fetchLapTime() {
    fetch('/getLapTime')
        .then(response => response.text())
        .then(data => {
            document.getElementById("lapTimeValue").innerText = parseFloat(data).toFixed(1); // Update lap time
        })
        .catch(error => console.error('Error fetching lap time:', error));
}

// Function to fetch running status
function fetchStatus() {
    fetch('/getStatus')
        .then(response => response.text())
        .then(data => {
            document.getElementById("status").innerText = data; // Update running status
        })
        .catch(error => console.error('Error fetching status:', error));
}



// Function to increment the lap (no page reload)
function incrementLap() {
    fetch('/incrementLap')
        .then(response => response.text())
        .then(data => {
            document.getElementById("lapValue").innerText = data;
        });
}

// Function to decrement the lap (no page reload)
function decrementLap() {
    fetch('/decrementLap')
        .then(response => response.text())
        .then(data => {
            document.getElementById("lapValue").innerText = data;
        });
}

// Function to increment the lap time (no page reload)
function incrementLapTime() {
    fetch('/incrementLapTime')
        .then(response => response.text())
        .then(data => {
            document.getElementById("lapTimeValue").innerText = data;
        });
}

// Function to decrement the lap time (no page reload)
function decrementLapTime() {
    fetch('/decrementLapTime')
        .then(response => response.text())
        .then(data => {
            document.getElementById("lapTimeValue").innerText = data;
        });
}

// Function to toggle the system status
function toggleSystem() {
    fetch('/toggleSystem')
        .then(response => response.text())
        .then(data => {
            document.getElementById("status").innerText = data; // Update displayed status
        });
}

function resetRealTimePacing() {
    fetch('/resetRealTimePacing')
}




// WebSocket to handle real-time data updates
var ws;

function initWebSocket() {
    ws = new WebSocket('ws://' + window.location.hostname + ':81/');
    ws.onmessage = function(event) {
        var data = JSON.parse(event.data);
        document.getElementById("status").innerText = data.status;
        document.getElementById("lapValue").innerText = data.lap;
        document.getElementById("lapTimeValue").innerText = data.lapTime.toFixed(1);
    };
}

window.onload = initWebSocket



