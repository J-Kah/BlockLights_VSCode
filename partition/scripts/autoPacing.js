document.addEventListener("DOMContentLoaded", function() {
    // Fetch the initial lap and lapTime values when the page is loaded
    fetchAutoStatus();
    fetchAutoLaps();
    fetchAutoTime();
    fetchAutoCountdown();
});


// Functions to fetch the current values from the server
function fetchAutoLaps() {
    fetch('/getAutoLaps')
        .then(response => response.text())
        .then(data => {
            document.getElementById("autoLaps").innerText = data; // Update lap value
        })
        .catch(error => console.error('Error fetching auto lap value:', error));
}

function fetchAutoTime() {
    fetch('/getAutoTime')
        .then(response => response.text())
        .then(data => {
            document.getElementById("autoTime").innerText = data; // Update lap value
        })
        .catch(error => console.error('Error fetching auto time value:', error));
}

function fetchAutoStatus() {
    fetch('/getAutoStatus')
        .then(response => response.text())
        .then(data => {
            document.getElementById("autoStatus").innerText = data; // Update running status
        })
        .catch(error => console.error('Error fetching auto status:', error));
}

function fetchAutoCountdown() {
    fetch('/getAutoCountdown')
        .then(response => response.text())
        .then(data => {
            document.getElementById("autoCountdown").innerText = data; // Update running status
        })
        .catch(error => console.error('Error fetching auto status:', error));
}

function toggleAutoCountdown() {
    fetch('/toggleAutoCountdown')
        .then(response => response.text())
        .then(data => {
            document.getElementById("autoCountdown").innerText = data;
        });
}

function toggleAutoPacing() {
    fetch('/toggleAutoPacing')
        .then(response => response.text())
        .then(data => {
            document.getElementById("autoStatus").innerText = data;
        });
}

function setAutoLaps() {
    fetch('/setAutoLaps', {
        method: 'POST', // Use POST to update the value
        headers: {
            'Content-Type': 'text/plain' // Change the content type to plain text
        },
        body: document.getElementById('autoLapsInput').value.toString() // Convert the float to a string
    })
    .then(response => response.text())
    .then(data => {
        document.getElementById("autoLaps").innerText = data;
    });
}


function setAutoTime() {
    var timeInput = document.getElementById('autoTimeInput').value;
    var timeRegex = /^([0-9]+):([0-5][0-9])\.([0-9]{2})$/;

    if (timeRegex.test(timeInput)) {
        var match = timeInput.match(timeRegex);
        // convert to float
        var minutes = parseInt(match[1], 10); // Extract minutes
        var seconds = parseInt(match[2], 10); // Extract seconds
        var milliseconds = parseInt(match[3], 10); // Extract milliseconds

        console.log(minutes);
        console.log(seconds);
        console.log(milliseconds);
        // Calculate total time in seconds as a float
        var totalTimeInSeconds = minutes * 60 + seconds + milliseconds / 100; // Convert milliseconds to seconds

        // Send the time to the server as a float directly
        fetch('/setAutoTime', {
            method: 'POST', // Use POST to update the value
            headers: {
                'Content-Type': 'text/plain' // Change the content type to plain text
            },
            body: totalTimeInSeconds.toString() // Convert the float to a string
        })
        .then(response => response.text()) // Assuming your server responds with plain text
        .then(data => {
            document.getElementById("autoTime").innerText = data;

        })
        .catch(error => {
            console.error('Error updating time:', error);
        });

    } else {
        alert("Invalid time format. Please use m:ss.xx");
    }
}

// WebSocket to handle real-time data updates
var ws;

function initWebSocket() {
    ws = new WebSocket('ws://' + window.location.hostname + ':81/');
    ws.onmessage = function(event) {
        var data = JSON.parse(event.data);
        document.getElementById("autoStatus").innerText = data.autoStatus;
        document.getElementById("autoLaps").innerText = data.autoLaps;
        document.getElementById("autoTime").innerText = data.autoTime;
        document.getElementById("autoCountdown").innerText = data.autoCountdown;
    };
}

window.onload = initWebSocket