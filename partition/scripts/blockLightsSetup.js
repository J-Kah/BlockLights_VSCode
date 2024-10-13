document.addEventListener("DOMContentLoaded", function() {
    // Fetch the initial lap and lapTime values when the page is loaded
    fetchBlockData();
});




// Function to load data from the ESP server
function fetchBlockData() {
    fetch('/getBlockData') // Replace with the correct endpoint on your ESP
        .then(response => response.json())
        .then(data => {
            const tableBody = document.getElementById('dataTable').getElementsByTagName('tbody')[0];
            tableBody.innerHTML = ''; // Clear existing rows

            // Loop through the array of structs and create table rows
            data.forEach((item, index) => {
                const row = tableBody.insertRow();
                const MACAddress = row.insertCell(0);
                const number = row.insertCell(1);
                const status = row.insertCell(2);

                MACAddress.textContent = item.MACAddress;
                status.textContent = item.status;

                // Add buttons if needed (e.g., manual blink, update, clear)
                const blinkButton = row.insertCell(3);
                const clearButton = row.insertCell(4);

                blinkButton.innerHTML = `<button onclick="blinkBlock('${item.MACAddress}')">Blink</button>`;

                if(item.number != 1){
                    number.innerHTML = `
                        ${item.number} 
                        <input type="number" id="updateInput_${item.MACAddress}" min="2" max="14" value="${item.number}">
                        <button onclick="sendUpdate('${item.number}', document.getElementById('updateInput_${item.MACAddress}').value)">Update</button>
                    `;
    
                    clearButton.innerHTML = `<button onclick="clearBlock('${item.MACAddress}')">Clear</button>`;
                } else {
                    number.textContent = item.number;
                    clearButton.textContent = "-";
                }

            });
        })
        .catch(error => console.error('Error fetching data:', error));
}

// Function to perform actions when buttons are clicked
function sendUpdate(numFrom, numTo) {
    if(numTo <= 14 && numTo >= 2) {
        fetch('/updateBlockNumber', {
            method: 'POST', // Use POST to update the value
            headers: {
                'Content-Type': 'text/plain' // Change the content type to plain text
            },
            body: JSON.stringify({ numFrom: numFrom, numTo: numTo })
        })
    }
}


function blockSetup(action) {
    fetch('/' + action) // Sends action request to ESP
        .then(response => response.text())
        .then(result => {
            console.log(result);
        })
        .catch(error => console.error('Error performing action:', error));
}

function clearBlock(mac){
    fetch('/clearBlock', {
        method: 'POST', // Use POST to update the value
        headers: {
            'Content-Type': 'text/plain' // Change the content type to plain text
        },
        body: mac // Convert the float to a string
    })
}

function blinkBlock(mac){
    fetch('/blinkBlock', {
        method: 'POST', // Use POST to update the value
        headers: {
            'Content-Type': 'text/plain' // Change the content type to plain text
        },
        body: mac
    })
}



// WebSocket to handle real-time data updates
var ws;

function initWebSocket() {
    ws = new WebSocket('ws://' + window.location.hostname + ':81/');

    ws.onmessage = function(event) {
        var data = JSON.parse(event.data);

        const tableBody = document.getElementById('dataTable').getElementsByTagName('tbody')[0];
        tableBody.innerHTML = ''; // Clear existing rows

        // Loop through the array of structs and create table rows
        data.forEach((item, index) => {
            const row = tableBody.insertRow();
            const MACAddress = row.insertCell(0);
            const number = row.insertCell(1);
            const status = row.insertCell(2);

            MACAddress.textContent = item.MACAddress;
            status.textContent = item.status;

            // Add buttons if needed (e.g., manual blink, update, clear)
            const blinkButton = row.insertCell(3);
            const clearButton = row.insertCell(4);

            blinkButton.innerHTML = `<button onclick="blinkBlock('${item.MACAddress}')">Blink</button>`;

            if(item.number != 1){
                number.innerHTML = `
                    ${item.number} 
                    <input type="number" id="updateInput_${item.MACAddress}" min="2" max="14" value="${item.number}">
                    <button onclick="sendUpdate('${item.number}', document.getElementById('updateInput_${item.MACAddress}').value)">Update</button>
                `;

                clearButton.innerHTML = `<button onclick="clearBlock('${item.MACAddress}')">Clear</button>`;
            } else {
                number.textContent = item.number;
                clearButton.textContent = "-";
            }

        });
    };
}

window.onload = initWebSocket;