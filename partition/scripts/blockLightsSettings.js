document.addEventListener("DOMContentLoaded", function() {
    // Fetch the initial lap and lapTime values when the page is loaded
    fetchTrackNumber();
    fetchNumberOfTracks();
    fetchStartingOn500mSide();
    fetchNumLeadingBlocks();
    fetchNumPacingBlocks();
    fetchNumTrailingBlocks();
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
        .catch(error => console.error('Error fetching numberOfTracks value:', error));
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

function fetchNumLeadingBlocks() {
    fetch('/getNumLeadingBlocks')
        .then(response => response.text())
        .then(data => {
            document.getElementById("numLeadingBlocks").innerText = data; // Update lap value
        })
        .catch(error => console.error('Error fetching NumLeadingBlocks value:', error));
}

function fetchNumPacingBlocks() {
    fetch('/getNumPacingBlocks')
        .then(response => response.text())
        .then(data => {
            document.getElementById("numPacingBlocks").innerText = data; // Update lap value
        })
        .catch(error => console.error('Error fetching NumPacingBlocks value:', error));
}

function fetchNumTrailingBlocks() {
    fetch('/getNumTrailingBlocks')
        .then(response => response.text())
        .then(data => {
            document.getElementById("numTrailingBlocks").innerText = data; // Update lap value
        })
        .catch(error => console.error('Error fetching NumTrailingBlocks value:', error));
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

function incrementNumLeadingBlocks() {
    fetch('/incrementNumLeadingBlocks')
        .then(response => response.text())
        .then(data => {
            document.getElementById("numLeadingBlocks").innerText = data;
        });
}

function decrementNumLeadingBlocks() {
    fetch('/decrementNumLeadingBlocks')
        .then(response => response.text())
        .then(data => {
            document.getElementById("numLeadingBlocks").innerText = data;
        });
}

function incrementNumPacingBlocks() {
    fetch('/incrementNumPacingBlocks')
        .then(response => response.text())
        .then(data => {
            document.getElementById("numPacingBlocks").innerText = data;
        });
}

function decrementNumPacingBlocks() {
    fetch('/decrementNumPacingBlocks')
        .then(response => response.text())
        .then(data => {
            document.getElementById("numPacingBlocks").innerText = data;
        });
}

function incrementNumTrailingBlocks() {
    fetch('/incrementNumTrailingBlocks')
        .then(response => response.text())
        .then(data => {
            document.getElementById("numTrailingBlocks").innerText = data;
        });
}

function decrementNumTrailingBlocks() {
    fetch('/decrementNumTrailingBlocks')
        .then(response => response.text())
        .then(data => {
            document.getElementById("numTrailingBlocks").innerText = data;
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