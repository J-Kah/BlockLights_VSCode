document.addEventListener("DOMContentLoaded", function() {
    // Fetch the initial lap and lapTime values when the page is loaded
    fetchLapValue();
    fetchLapTime();
    fetchStatus();
});

let lastRequestTime = 0;
const throttleTime = 200; // Time in milliseconds (0.2 seconds)

function reqAllowed(){
    const currentTime = Date.now();
    // Check if enough time has passed since the last request
    if (currentTime - lastRequestTime >= throttleTime) {
        lastRequestTime = currentTime; // Update the last request time
        return true;
    }
    return false;
}

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
    if(reqAllowed()) {
        fetch('/incrementLap')
            .then(response => response.text())
            .then(data => {
                document.getElementById("lapValue").innerText = data;
            });
    }
}

// Function to decrement the lap (no page reload)
function decrementLap() {
    if(reqAllowed()) {
        fetch('/decrementLap')
            .then(response => response.text())
            .then(data => {
                document.getElementById("lapValue").innerText = data;
            });
    }
}

// Function to increment the lap time (no page reload)
function incrementLapTime() {
    if(reqAllowed()) {
        fetch('/incrementLapTime')
            .then(response => response.text())
            .then(data => {
                document.getElementById("lapTimeValue").innerText = data;
            });
    }
}

// Function to decrement the lap time (no page reload)
function decrementLapTime() {
    if(reqAllowed()) {
        fetch('/decrementLapTime')
            .then(response => response.text())
            .then(data => {
                document.getElementById("lapTimeValue").innerText = data;
            });
    }
}

// Function to toggle the system status
function toggleSystem() {
    if(reqAllowed()) {
        fetch('/toggleSystem')
            .then(response => response.text())
            .then(data => {
                document.getElementById("status").innerText = data; // Update displayed status
            });
    }
}

function resetRealTimePacing() {
    if(reqAllowed()) {
        fetch('/resetRealTimePacing')
    }
}




// WebSocket to handle real-time data updates
var ws;

function initWebSocket() {
    ws = new WebSocket('ws://' + window.location.hostname + ':81/');
    ws.onmessage = function(event) {
        var data = JSON.parse(event.data);
        document.getElementById("status").innerText = data.status;
        console.log("This is the status");
        console.log(data.status);
        document.getElementById("lapValue").innerText = data.lap;
        console.log("This is the lap");
        console.log(data.lap);
        document.getElementById("lapTimeValue").innerText = data.lapTime;
        console.log("This is the lapTime");
        console.log(data.lapTime);

        // Update the circles based on the received data
        data.circles.forEach(item => {
            const circle = document.getElementById(item.id);
            if (circle) {
                circle.style.backgroundColor = item.color;
            }
        });
    };
}

window.onload = initWebSocket



