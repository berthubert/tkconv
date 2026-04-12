"use strict";

async function updateInterval(f, id, newint)
{
    console.log(id, newint);

    const formData = new FormData();
    formData.append('id', id);

    formData.append('interval', newint);

    const response = await fetch('set-monitor-interval', { method: "POST", body: formData });
    if (response.ok === true) {
        const data = await response.json();
	f.msg[id]="Aangepast";
    }
    else {
	console.log("error");
    }

    return false;
}

async function updateAllInterval(f, newint)
{
    const formData = new FormData();
    formData.append('interval', newint);

    const response = await fetch('set-my-monitors-interval', { method: "POST", body: formData });
    if (response.ok === true) {
        const data = await response.json();
	for (let e of f.monitors) {
	    e.interval = newint;
	}
	f.allinterval="";
    }
    else {
	console.log("error");
    }

    return false;
}


async function doMijnInit(f)
{
    let url = new URL(window.location.href)
    if(url.searchParams.get("id") != null) {
	f.id = url.searchParams.get("id");
	f.state = 2;
	console.log("Set state to 2");
    }
    else if(url.searchParams.get("session") != null) {
	f.session = url.searchParams.get("session");
	f.state = 5;
	// remove the 'session=' from the URL
	const url2 = new URL("mijn.html", window.location.href);
	history.pushState({}, "", url2);
    }
    else {
	const response = await fetch('status');
	if (response.ok === true) {
            const data = await response.json();
	    console.log(data);
	    if(data.login === true) {
		f.state=4;
		f.email=data.email;
	    }
	    else
		f.state = 0;
	}
	else {
	    console.log("error");
	}
    }
}



async function getMonitors(f)
{
    getOOVerantwoordelijken(f);
    getOOMonitors(f);

    
    const response = await fetch('my-monitors');
    if (response.ok === true) {
        const data = await response.json();
	console.log(data);
	f.monitors = data["monitors"];
    }

}

async function removeOOMonitorAndSync(f, v)
{
    console.log(`Would like to remove "${v}"`);
    const formData = new FormData();
    formData.append('verantwoordelijke', v);

    const response = await fetch('remove-oo-verantwoordelijke-monitor', { method: "POST", body: formData });
    if (response.ok === true) {
        const data = await response.json();
	f.verantwoordelijken = data["oomonitors"];
	console.log(data);
	
	// sync these too
	const response2 = await fetch('my-monitors');
	if (response2.ok === true) {
            const data = await response2.json();
	    console.log(data);
	    f.monitors = data["monitors"];
	}

	
    }
    else {
	console.log("error");
    }


    
    return false;
}

async function addOOMonitorAndSync(f, v)
{
    console.log(`Would like to add "${v}"`);
    if(f.verantwoordelijken.indexOf(v) >= 0) {
	console.log("Was there already, not doing a thing");
	return;
    }
    else
	console.log(f.verantwoordelijken);
    
    const formData = new FormData();
    formData.append('verantwoordelijke', v);

    const response = await fetch('add-oo-verantwoordelijke-monitor', { method: "POST", body: formData });
    if (response.ok === true) {
        const data = await response.json();
	f.verantwoordelijken = data["oomonitors"];

	// sync these too
	const response2 = await fetch('my-monitors');
	if (response2.ok === true) {
            const data = await response2.json();
	    console.log(data);
	    f.monitors = data["monitors"];
	}
	
	console.log(data);
    }
    else {
	console.log("error");
    }
    return false;
}

async function getOOMonitors(f)
{
    const response = await fetch('get-oo-verantwoordelijke-monitors');
    if (response.ok === true) {
        const data = await response.json();
	f.verantwoordelijken = data["oomonitors"];
	console.log(data);
    }
    else {
	console.log("error");
    }
    return false;
}

async function doRequestInvite(f)
{
    const formData = new FormData();
    formData.append('email', f.email);

    const response = await fetch('create-user-invite', { method: "POST", body: formData });
    if (response.ok === true) {
        const data = await response.json();
	console.log(data);
	f.state = 1;
    }
    else {
	console.log("error");
    }

    return false;
}

async function doConfirmInvite(f)
{
    const formData = new FormData();
    formData.append('id', f.id);

    const response = await fetch('confirm-user-invite', { method: "POST", body: formData });
    if (response.ok === true) {
        const data = await response.json();
	console.log(data);
	f.state = 3;
	f.email = data.email;
	// remove the 'id=' from the URL
	const url = new URL("mijn.html", window.location.href);
	history.pushState({}, "", url);
    }
    else {
	console.log("error");
    }

    return false;
}


async function doConfirmRejoin(f)
{
    const response = await fetch('join-session/'+f.session, { method: "POST"});
    if (response.ok === true) {
        const data = await response.json();
	console.log(data);
	if(data.ok == 1) {
	    f.state = 4;
	    f.email = data.email;
	}
	else {
	    window.alert("Deze login-link was al eens gebruikt, vraag een nieuwe aan!");
	    window.location.href = "mijn.html";		
	}
    }
    else {
	console.log("error joining session");
    }
}

async function removeMonitorAndUpdate(f, id)
{
    console.log(`Should remove monitor ${id}`);
    const formData = new FormData();
    formData.append('id', id);

    const response = await fetch('remove-monitor', { method: "POST", body: formData });
    if (response.ok === true) {
        const data = await response.json();
	console.log(data);
	return getMonitors(f);
    }
    else {
	console.log("error");
    }
    return false;
}

async function removeMonitor(f)
{
    console.log(`Should remove monitor ${f.id}`);
    const formData = new FormData();
    formData.append('id', f.monitorId);

    const response = await fetch('remove-monitor', { method: "POST", body: formData });
    if (response.ok === true) {
        const data = await response.json();
	f.haveMonitor=false;
	f.monitorId="";
    }
    else {
	console.log("error");
    }
}

// if zaakFromDocument is passed and true, this will override the 'document' kind & add the zaaknummer
async function checkMonitor(f, zaakFromDocument) 
{
    const response = await fetch('status');
    if (response.ok === true) {
        const data = await response.json();
	if(data.login === true ) {
	    let url = 'have-monitor/';
	    if(zaakFromDocument == true)
		url += `zaak/${f.zaaknummer}`;
	    else
		url += `${f.kind}/${f.nummer}`;

	    const response2 = await fetch(url);
	    if (response2.ok === true) {
		const data = await response2.json();
		console.log("have-monitor: ")
		f.haveMonitor=  data["have"] === 1;
		f.monitorId = data["id"];
	    }
	    else
		f.haveMonitor = false;
	}
	else
	    f.haveMonitor = false;
    }
   
}

// this 'ign' is so we can pass haveMonitor as a parameter, causing alpine.js to call this function
// when haveMonitor changes!

// this is the main entry function to draw the bell, it will chooose the color and if it is active
// if the user is not logged in, it sends the user to the login page
async function makeBell(f, ign, zaaknummer)
{
    const response = await fetch('status');
    if (response.ok === true) {
        const data = await response.json();
	if(data.login === true) {
	    return doMakeBell(f, zaaknummer)
	}
	
    }
    else {
	console.log("error");
    }

    // this is the bell we show to someone not logged in
    let func = "window.location='mijn.html';";
    f.haveMonitor = false;
    return `<svg @click="${func}" id="belletje" fill="#999999" height="48px" width="48px" version="1.1" id="Layer_1" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" 
	 viewBox="0 0 300 300" xml:space="preserve">
<g>
	<g>
		<path d="M149.996,0C67.157,0,0.001,67.161,0.001,149.997S67.157,300,149.996,300s150.003-67.163,150.003-150.003
			S232.835,0,149.996,0z M149.999,232.951c-10.766,0-19.499-8.725-19.499-19.499h38.995
			C169.497,224.226,160.765,232.951,149.999,232.951z M215.889,193.9h-0.005v-0.001c0,7.21-5.843,7.685-13.048,7.685H97.16
			c-7.208,0-13.046-0.475-13.046-7.685v-1.242c0-5.185,3.045-9.625,7.42-11.731l4.142-35.753c0-26.174,18.51-48.02,43.152-53.174
			v-13.88c0-6.17,5.003-11.173,11.176-11.173c6.17,0,11.173,5.003,11.173,11.173V92c24.642,5.153,43.152,26.997,43.152,53.174
			l4.142,35.758c4.375,2.109,7.418,6.541,7.418,11.726V193.9z"/>
	</g>
</g>
</svg>`;
}

// this is the active bell, for someone who is logged in
function doMakeBell(f, zaaknummer)
{
    console.log(`Makebell called haveMonitor: "${f.haveMonitor}", monitorID: "${f.monitorId}"`);
    let color="";
    let func="";
    if(f.haveMonitor) {
	color='#ff0000';
	func = "removeMonitor($data)";
    }
    else {
	color='#999999';
	
	if(f.kind == "persoon")
	    func = "addPersoonMonitor($data)";
	else if(f.kind == "commissie")
	    func = "addCommissieMonitor($data)";
	else if(f.kind == "activiteit")
	    func = "addActiviteitMonitor($data)";
	else if(f.kind == "zoek")
	    func = "addZoekMonitor($data)";
	else if(f.kind == "zaak")
	    func = "addZaakMonitor($data)";
	else if(f.kind == "document")
	    func = `addZaakMonitor($data, '${zaaknummer}')`; // this passes in the right zaaknummer from the document page
	else if(f.kind == "ksd")
	    func = "addKsdMonitor($data)";
	else if(f.kind == "toezeggingen")
	    func = "addToezeggingenMonitor($data)";
    }
    
    return `<svg @click="${func}" id="belletje" fill="${color}" height="48px" width="48px" version="1.1" id="Layer_1" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" 
	 viewBox="0 0 300 300" xml:space="preserve">
<g>
	<g>
		<path d="M149.996,0C67.157,0,0.001,67.161,0.001,149.997S67.157,300,149.996,300s150.003-67.163,150.003-150.003
			S232.835,0,149.996,0z M149.999,232.951c-10.766,0-19.499-8.725-19.499-19.499h38.995
			C169.497,224.226,160.765,232.951,149.999,232.951z M215.889,193.9h-0.005v-0.001c0,7.21-5.843,7.685-13.048,7.685H97.16
			c-7.208,0-13.046-0.475-13.046-7.685v-1.242c0-5.185,3.045-9.625,7.42-11.731l4.142-35.753c0-26.174,18.51-48.02,43.152-53.174
			v-13.88c0-6.17,5.003-11.173,11.176-11.173c6.17,0,11.173,5.003,11.173,11.173V92c24.642,5.153,43.152,26.997,43.152,53.174
			l4.142,35.758c4.375,2.109,7.418,6.541,7.418,11.726V193.9z"/>
	</g>
</g>
</svg>`;
}


async function addKsdMonitor(f)
{
    if(f.haveMonitor)
	return false;
 
    console.log(`Should add ksd ${f.nummer} ${f.toevoeging}`);
    const formData = new FormData();
    formData.append('nummer', f.nummer);
    formData.append('toevoeging', f.toevoeging);

    const response = await fetch('add-ksd-monitor', { method: "POST", body: formData });
    if (response.ok === true) {
        const data = await response.json();
	console.log(data);
	f.haveMonitor = true;
	f.monitorId = data["id"];
	console.log(f.monitorId);
    }
    else {
	console.log("error");
    }
    return false;
}

async function addToezeggingenMonitor(f)
{
    if(f.haveMonitor)
	return false;
 
    console.log(`Should add toezeggingen ${f.nummer} ${f.fractie} ${f.voortouw}`);
    const formData = new FormData();
    formData.append('fractie', f.fractie);
    formData.append('voortouw', f.voortouw);

    const response = await fetch('add-toezeggingen-monitor', { method: "POST", body: formData });
    if (response.ok === true) {
        const data = await response.json();
	console.log(data);
	f.haveMonitor = true;
	f.monitorId = data["id"];
	console.log(f.monitorId);
    }
    else {
	console.log("error");
    }
    return false;
}


async function addZoekMonitor(f)
{
    console.log(`Should add zoek ${f.query}`);
    const formData = new FormData();
    formData.append('query', f.searchQuery);
    formData.append('categorie', 'alles');

    const response = await fetch('add-search-monitor', { method: "POST", body: formData });
    if (response.ok === true) {
        const data = await response.json();
	console.log(data);
	f.haveMonitor = true;
    }
    else {
	console.log("error");
    }
    return false;
}


async function addXMonitor(f, kind, nummer)
{
    console.log(`Should add ${kind} ${nummer}, haveMonitor = ${f.haveMonitor}`);
    if(f.haveMonitor)
	return false;
    
    const formData = new FormData();
    formData.append('nummer', nummer);

    const response = await fetch(`add-${kind}-monitor`, { method: "POST", body: formData });
    if (response.ok === true) {
        const data = await response.json();
	console.log(data);
	f.haveMonitor = true;
	f.monitorId = data["id"];
    }
    else {
	console.log("error");
    }
    return false;
}


async function addCommissieMonitor(f)
{
    return addXMonitor(f, "commissie", f.cid);
}
async function addActiviteitMonitor(f)
{
    return addXMonitor(f, "activiteit", f.nummer);
}
async function addZaakMonitor(f, zaaknummer)
{
    console.log(zaaknummer);
    if(zaaknummer == undefined)
	return addXMonitor(f, "zaak", f.nummer);
    else
	return addXMonitor(f, "zaak", zaaknummer);
}

async function addPersoonMonitor(f)
{
    return addXMonitor(f, "person", f.nummer);
}

async function doLogout()
{
    const response = await fetch('logout', { method: "POST"});
    if (response.ok === true) {
        const data = await response.json();
	window.location="mijn.html";
    }
    else {
	console.log("error");
    }
}

