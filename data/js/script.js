//var set_button = document.getElementById("set_button")
//var
let IP = "192.168.4.1"
let chart;
let startCharting = false
const ws = new WebSocket("ws://" + IP + "/ws")
document.getElementById("impulse").textContent = Number(0).toFixed(3)
document.getElementById("SD").textContent = 0;
document.getElementById("cont").textContent = 0;

async function cmd(action){
    if(action == 'set'){
        const user_input = await prompt("Enter the known calibration value: ")
        if (user_input !== null){

            alert("Calibration value set to: " + user_input)
            fetch('/cmd?action=set&value='+user_input)
            .then(response => response.json())
            .then(data => {
                document.getElementById("impulse").textContent = data.calibrationvalue.toFixed(3);}
            )
        } 
    }
    else if(action == 'calibrate'){
        
        const user_input = await prompt("Enter the known weight: ")
        if(!user_input || isNaN(user_input) || user_input <= 0) return;

        await fetch('/cmd?action=calibrate&weight='+user_input)
        if(!confirm("Remove weight from scale")) return;
        await fetch('/cmd?action=calibrate&task=tare')
        alert("Tare Done")
        if(!confirm("Place known weight on scale")) return;
        alert("Now measuring")
        const resp = await fetch('/cmd?action=calibrate&task=calibrate')
        const result = await resp.text()

        alert("Calibration DONE")

    }
    else if(action == 'test'){
        const user_input = await prompt("Enter the file name: ")
        if(user_input !== null){
            alert('File named: ' + user_input)
            console.log('/cmd?action='+ action+'&name='+user_input)
        }

        await fetch('/cmd?action='+ action+'&name='+user_input)
        .then(response => response.json())
        .then(data => {
            document.getElementById("SD").textContent = data.SD;}
        )
    }
    else if(action == 'ignite'){
        await fetch("/cmd?action=ignite");
        document.getElementById("overlay").style.display = "flex"; 
        
        startCountDown()


}

    else if(action == "stop"){
        await fetch("/cmd?action=stop");
        startCharting =  false
    }

    //     setInterval( async () => {
    //     response =  await fetch('/impulse')
    //     impulse_json =  await response.json()
    //     document.getElementById("Impulse").textContent = impulse_json.impulse
    // }, 50 )
}


// setInterval(async () => {

//     response = await fetch('/cont')
//     cont_json = await response.json()
//     document.getElementById("cont").textContent = cont_json.cont

// }, 200)
ws.onopen = () => {
    const ctx = document.getElementById("ThrustGraph").getContext("2d")
    chart = new Chart(ctx, {
        type: "line",
        data: {
            labels: [],
            datasets: [{
                label: 'Thrust (N)',
                data: [],
                borderColor: '#000',
                backgroundColor: 'rgba(0,255,0,0.1)',
                fill: 'true',
                tension: 0.1
            }]
        },
        options: {
            animation: false,
            scales: {
                x: {title: {display: true, text: "Time (ms)"}},
                y: {title: {display: true, text: "Thrust (KG)"}},
            }
        }
    })
}

ws.onmessage = (e) => {
const d = JSON.parse(e.data)
document.getElementById("Thrust").textContent = Number(d.thrust).toFixed(3)


if(startCharting){
    chart.data.labels.push(d.time)
    chart.data.datasets[0].data.push(d.thrust)

    chart.update('quiet')
}
}

function startCountDown(){
    let n = 10
    const timer = setInterval(() => {
        document.getElementById("timer").textContent = Number(n)
        if(n==0){
            // alert("FIRE")
            document.getElementById("timer").textContent = "FIRE"
            fetch("/cmd?action=go")
            startCharting = true
            document.getElementById("overlay").style.display = "none"
        }
        
        if(n<=-1){
            clearInterval(timer)
        }
        n--
    },1000)

}
