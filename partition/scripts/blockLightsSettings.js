document.addEventListener("DOMContentLoaded", function() {
    // Fetch the initial lap and lapTime values when the page is loaded
    fetchTrackNumber();
    fetchNumberOfTracks();
    fetchStartingOn500mSide();
});

// Function to fetch the current lap value from the server
function fetchTrackNumber() {
    fetch('/getTrackNumber')
        .then(response => response.text())
        .then(data => {
            document.getElementById("trackNumber").innerText = data; // Update lap value
        })
        .catch(error => console.error('Error fetching trackNumber value:', error));
}

// Function to fetch the current lap value from the server
function fetchNumberOfTracks() {
    fetch('/getNumberOfTracks')
        .then(response => response.text())
        .then(data => {
            document.getElementById("numberOfTracks").innerText = data; // Update lap value
        })
        .catch(error => console.error('Error fetching NnumberOfTracks value:', error));
}

// Function to fetch the current lap value from the server
function fetchStartingOn500mSide() {
    fetch('/getStartingOn500mSide')
        .then(response => response.text())
        .then(data => {
            document.getElementById("startingOn500mSide").innerText = data; // Update lap value
        })
        .catch(error => console.error('Error fetching startingOn500mSide value:', error));
}

function incrementTrackNumber() {
    fetch('/incrementTrackNumber')
        .then(response => response.text())
        .then(data => {
            console.log('Track Number incremented:', data); // Log the response
            document.getElementById("trackNumber").innerText = data;
        });
}

function decrementTrackNumber() {
    fetch('/decrementTrackNumber')
        .then(response => response.text())
        .then(data => {
            document.getElementById("trackNumber").innerText = data;
        });
}

function toggleNumberOfTracks() {
    fetch('/toggleNumberOfTracks')
        .then(response => response.text())
        .then(data => {
            document.getElementById("numberOfTracks").innerText = data;
        });
}

function toggleStartingOn500mSide() {
    fetch('/toggleStartingOn500mSide')
        .then(response => response.text())
        .then(data => {
            document.getElementById("startingOn500mSide").innerText = data;
        });
}

// WebSocket to handle real-time data updates
var ws;

function initWebSocket() {
    ws = new WebSocket('ws://' + window.location.hostname + ':81/');
    ws.onmessage = function(event) {
        var data = JSON.parse(event.data);
        document.getElementById("trackNumber").innerText = data.trackNumber;
    };
}

window.onload = initWebSocket