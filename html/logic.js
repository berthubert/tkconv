"use strict";

async function getGen(orig, dest, f) {
    
    const response = await fetch(orig);
    if (response.ok === true) {
        const data = await response.json();
        f[dest] = data;
    }
}

async function getGenKSD(orig, dest, f)
{
    const response = await fetch(orig);
    if (response.ok === true) {
        const data = await response.json();
        f[dest] = data;
	var ksdnamen=[]
	for (const element of data) {
	    if(element.toevoeging != '')
		ksdnamen.push(element.nummer +"-" + element.toevoeging +" " +element.titel);
	    else
		ksdnamen.push(element.nummer +" " +element.titel);
	}
	var config= {
	    placeHolder: "Kamerstukdossier titel",
	    threshold: 2,
	    data: {
		src: async (query) => {
		    try {
			// Fetch Data from external Source
			const searchParams = new URLSearchParams("");
			searchParams.set("q", query);
			const source = await fetch("./ksds?"+searchParams.toString());
			const data = await source.json();
			return data;
		    } catch (error) {
			console.log(error);
			return error;
		    }
		}


	    },
	    diacritics: true,

	    resultItem: {
		highlight: {
		    render: true
		},
		submit: true
	    },
	    resultsList: {
		maxResults: 100,
		element: (list, data) => {
		    const info = document.createElement("p");
		    if (data.results.length) {
			info.innerHTML = `Toont <strong>${data.results.length}</strong> van de <strong>${data.matches.length}</strong> resultaten`;
		    } else {
			info.innerHTML = `Vond <strong>${data.matches.length}</strong> documenten voor <strong>"${data.query}"</strong>`;
		    }
		    list.prepend(info);
		    
		}
	    },
	    events: {
		input: {
		    selection: (event) => {
			const selection = event.detail.selection.value;
			autoCompleteJS.input.value = selection;
			let selected = selection.split(" ")[0];
			// https://berthub.eu/tkconv/ksd.html?ksd=25424
			let fspl = selected.split("-");
			const num = fspl[0];
			let toevoeging="";

			if(fspl.length > 1)
			    toevoeging = fspl[1];
			window.location.href = "ksd.html?ksd="+selected+"&toevoeging=" + toevoeging;
		    }
		}
	    }
	};
    }
    const autoCompleteJS = new autoComplete(config);
}

async function getGenVragen(orig, dest, f)
{
    const response = await fetch(orig);
    if (response.ok === true) {
        const data = await response.json();
        f[dest] = data;
	var ksdnamen=[]
	for (const element of data) {
	    ksdnamen.push(element.nummer +" " + element.onderwerp);
	}
	var config= {
	    placeHolder: "Vraag",
	    data: {
		src: ksdnamen
	    },
	    diacritics: true,

	    resultItem: {
		highlight: {
		    render: true
		},
		submit: true
	    },
	    resultsList: {
		maxResults: 1000,
		element: (list, data) => {
		    const info = document.createElement("p");
		    if (data.results.length) {
			info.innerHTML = `Displaying <strong>${data.results.length}</strong> out of <strong>${data.matches.length}</strong> results`;
		    } else {
			info.innerHTML = `Found <strong>${data.matches.length}</strong> matching results for <strong>"${data.query}"</strong>`;
		    }
		    list.prepend(info);
		    
		}
	    },
	    events: {
		input: {
		    selection: (event) => {
			const selection = event.detail.selection.value;
			autoCompleteJS.input.value = selection;
			selected = selection.split(" ")[0];
			// https://berthub.eu/tkconv/ksd.html?ksd=25424
			window.location.href = "zaak.html?nummer="+selected;
		    }
		}
	    }
	};
    }
    const autoCompleteJS = new autoComplete(config);
}

let dateDescending = true;
function orderByDate(f, flip)
{
    if(flip)
    	dateDescending = !dateDescending;
    f.foundDocs=f.foundDocs.sort(function(a,b) {
	if(a.datum < b.datum)
	    return dateDescending ? 1 : -1;
	if(a.datum > b.datum)
	    return dateDescending ? -1 : 1;
	return 0;
    });

    let columnText = dateDescending ? "Datum &#x25BC" : "Datum &#x25B2";
    // this is a somewhat silly hack, but it gets the job done on initial load
    if(document.querySelectorAll("table.striped > thead > tr > th > a").length != 0) {
	    document.querySelectorAll("table.striped > thead > tr > th > a")[0].innerHTML = columnText;
    }

}

function orderByScore(f)
{
    f.foundDocs=f.foundDocs.sort(function(a,b) {
	return a.score - b.score;
    });

    document.querySelectorAll("table.striped > thead > tr > th > a")[0].innerText = "Datum";
}

// for search
function init(f)
{
    let url = new URL(window.location.href)
    f.searchQuery = url.searchParams.get("q");
    if(url.searchParams.get("twomonths")=="true")
	f.twomonths=true;
    else
	f.twomonths=false;
    if(url.searchParams.get("soorten") != null)
	f.soorten = url.searchParams.get("soorten");
    else
	f.soorten= "alles";

    if(f.searchQuery != null) {
	getSearchResults(f);
    }
}

async function commissieinit(f)
{
    let url = new URL(window.location.href)
    f.cid = url.searchParams.get("id");
    const response = await fetch('commissie/'+f.cid);
    if (response.ok === true) {
        const data = await response.json();
	f.cdata = data;
	f.loaded=true;
    }

    const response2 = await fetch('have-monitor/commissie/'+f.cid);
    if (response2.ok === true) {
        const data = await response2.json();
	f.haveMonitor = data["have"] === 1;
	f.monitorId = data["id"];
	console.log(`data["have"] = ${data.have}, we just set f.haveMonitor = ${f.haveMonitor}`);
    }
    else {
	console.log(`Mis commissie ${f.cid}`);
    }
    
    
}

async function activiteitinit(f)
{
    let url = new URL(window.location.href)
    f.nummer = url.searchParams.get("nummer");
    return getActiviteitDetails(f);
}

async function getActiviteitDetails(f)
{
    const response = await fetch('activiteit/'+f.nummer);
    if (response.ok === true) {
        const data = await response.json();
        f["activiteit"] = data;
	f["loaded"]=true;
    }

    const response2 = await fetch('have-monitor/activiteit/'+f.nummer);
    if (response2.ok === true) {
        const data = await response2.json();
	f.haveMonitor = data["have"] === 1;
	f.monitorId = data["id"];
    }
    else {
	console.log("Mis - no monitor or error");
    }
}

async function ksdinit(f)
{
    let url = new URL(window.location.href)
    f.nummer = url.searchParams.get("ksd");
    f.toevoeging = url.searchParams.get("toevoeging");

    const response2 = await fetch('have-monitor/ksd/'+f.nummer+"?toevoeging="+f.toevoeging);
    if (response2.ok === true) {
        const data = await response2.json();
	f.haveMonitor = data["have"] === 1;
	f.monitorId = data["id"];
	console.log(`data["have"] = ${data.have}`);
    }
    else {
	console.log("Mis");
    }
}

async function getSearchResults(f)
{
    if(f.searchQuery == '' || f.searchQuery == null || f.busy)
	return;

    f.searchQuery = f.searchQuery.replace(/[\u201C\u201D]/g, '"'); 
    //    “smart quotes” ->  "smart quotes" 
    f.busy = true;
    f.rssurl = "";
    f.haveMonitor = false;

    const url = new URL(window.location.href);
    url.searchParams.set("q", f.searchQuery);
    url.searchParams.set("twomonths", f.twomonths);
    url.searchParams.set("soorten", f.soorten);
    history.pushState({}, "", url);
    f.alternatief='';
    f.message='';
    if(/^2[0-9][0-9][0-9]Z[0-9]*$/.test(f.searchQuery)) {
	console.log("zaak match");
	f.alternatief = `<p><em>Bedoelt u mogelijk zaak <a href="zaak.html?nummer=${f.searchQuery}">${f.searchQuery}</a>?</em></p>`;
    }
    if(/^2[0-9][0-9][0-9]A[0-9]*$/.test(f.searchQuery)) {
	console.log("zaak match");
	f.alternatief = `<p><em>Bedoelt u mogelijk activiteit <a href="activiteit.html?nummer=${f.searchQuery}">${f.searchQuery}</a>?</em></p>`;
    }
    
    else if(/^2[0-9][0-9][0-9]D[0-9]*$/.test(f.searchQuery)) {
	f.alternatief = `<p><em>Bedoelt u mogelijk document <a href="get/${f.searchQuery}">${f.searchQuery}</a>?</em></p>`;
    }
    else if(/^[0-9][0-9][0-9][0-9][0-9]$/.test(f.searchQuery)) {
	f.alternatief = `<p><em>Bedoelt u mogelijk kamerstukdossier <a href="ksd.html?ksd=${f.searchQuery}&toevoeging=">${f.searchQuery}</a>?</em></p>`;
    }
    else if(/^kst-[0-9-]+$/.test(f.searchQuery)) {
	f.alternatief = `<p><em>Bedoelt u mogelijk kamerstuk <a href="op/${f.searchQuery}">${f.searchQuery}</a>?</em></p>`;
    }
    
    const formData = new FormData();
    formData.append('q', f.searchQuery);
    formData.append('twomonths', f.twomonths);
    formData.append('soorten', f.soorten);

    const response = await fetch('search', { method: "POST", body: formData });
    if (response.ok === true) {
        const data = await response.json();
        f.foundDocs = data["results"];
	const rssurl = new URL("https://berthub.eu/tkconv/search/index.xml");
	rssurl.searchParams.set("q", f.searchQuery);
	f.rssurl = rssurl.href

	f.message = `${data["milliseconds"]} milliseconden`;
	f.busy=false;
	orderByDate(f, false);
    }
    else {
	f.foundDocs=[];
	f.message = `Geen resultaten - probeer "${f.searchQuery}"`;
	f.busy=false;
    }
}


