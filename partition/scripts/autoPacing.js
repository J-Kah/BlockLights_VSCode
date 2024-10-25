document.addEventListener("DOMContentLoaded", function() {
    // Fetch the initial lap and lapTime values when the page is loaded
    fetchAutoStatus();
    fetchAutoLaps();
    fetchAutoTime();
    fetchAutoCountdown();
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
    if(reqAllowed()) {
    fetch('/toggleAutoCountdown')
        .then(response => response.text())
        .then(data => {
            document.getElementById("autoCountdown").innerText = data;
        });
    }
}

function toggleAutoPacing() {
    if(reqAllowed()) {
        fetch('/toggleAutoPacing')
            .then(response => response.text())
            .then(data => {
                document.getElementById("autoStatus").innerText = data;
            });
    }
}

function setAutoLaps() {
    if(reqAllowed()) {
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
}


function setAutoTime() {
    if(reqAllowed()) {
        var timeInput = document.getElementById('autoTimeInput').value;

        // Regular expressions for different allowed formats
        var secondsRegex = /^([0-9]+(\.[0-9]+)?)$/; // seconds or seconds with decimals
        var secondsWithMsRegex = /^([0-9]+):([0-5]?[0-9])(\.[0-9]{1,2})?$/; // minutes:seconds or minutes:seconds with decimals
        var totalTimeInSeconds = 0;

        // Check for seconds or seconds with milliseconds
        if (secondsRegex.test(timeInput)) {
            var match = timeInput.match(secondsRegex);
            totalTimeInSeconds = parseFloat(match[1]); // Directly convert to float
        } 
        // Check for minutes:seconds or minutes:seconds with milliseconds
        else if (secondsWithMsRegex.test(timeInput)) {
            var match = timeInput.match(secondsWithMsRegex);
            var minutes = parseInt(match[1], 10); // Extract minutes
            var seconds = parseInt(match[2], 10); // Extract seconds, may need padding
            var milliseconds = match[3] ? parseFloat(match[3]) : 0; // Extract milliseconds if present

            // Pad the seconds if necessary (e.g., turn "1:1" into "1:01")
            if (seconds < 10) {
                seconds = "0" + seconds;
            }

            // Calculate total time in seconds as a float
            totalTimeInSeconds = minutes * 60 + parseFloat(seconds) + (milliseconds ? milliseconds : 0);
        } else {
            alert("Invalid time format. Please use: seconds, seconds with decimals, m:ss, or m:ss.xx");
            return; // Exit the function if the format is invalid
        }

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
        document.getElementById("autoCountdown").innerText = data.autoCountdown;

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