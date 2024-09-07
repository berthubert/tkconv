function recentInit(f)
{
    setInterval(getRecentDocs, 60000, f);
    getRecentDocs(f);
}

async function getRecentDocs(f) {
    
    const response = await fetch('recent-docs');
    if (response.ok === true) {
        const data = await response.json();
        f.recentDocs = data;
	console.log("Update!");
    }
}

async function getRecentActiviteiten(f) {
    
    const response = await fetch('future-activities');
    if (response.ok === true) {
        const data = await response.json();
        f.recentActiviteiten = data;
    }
}

async function getOngeplandeActiviteiten(f) {
    
    const response = await fetch('unplanned-activities');
    if (response.ok === true) {
        const data = await response.json();
        f.ongeplandeActiviteiten = data;
    }
}


async function getOpenToezeggingen(f) {
    
    const response = await fetch('open-toezeggingen');
    if (response.ok === true) {
        const data = await response.json();
        f.openToezeggingen = data;
    }
}


function init(f)
{
    let url = new URL(window.location.href)
    f.searchQuery = url.searchParams.get("q");
    if(url.searchParams.get("twomonths")=="true")
	f.twomonths=true;
    else
	f.twomonths=false;
    if(f.searchQuery != null)
	getSearchResults(f);
}

async function getSearchResults(f)
{
    const url = new URL(window.location.href);
    url.searchParams.set("q", f.searchQuery);
    url.searchParams.set("twomonths", f.twomonths);
    history.pushState({}, "", url);
    
    const formData = new FormData();
    formData.append('q', f.searchQuery);
    formData.append('twomonths', f.twomonths);

    const response = await fetch('search', { method: "POST", body: formData });
    if (response.ok === true) {
        const data = await response.json();
        f.foundDocs = data["results"];
	
	f.message = `${data["milliseconds"]} milliseconden`;
	console.log(data);
    }
    else {
	f.foundDocs=[];
	f.message = `Geen resultaten - probeer "${f.searchQuery}"`;
    }
}
