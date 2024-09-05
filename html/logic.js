async function getRecentDocs(f) {
    const response = await fetch('recent-docs');
    if (response.ok === true) {
        const data = await response.json();
        f.recentDocs = data;
	console.log("It worked!");
	console.log(f.recentDocs);
	console.log(data);
    }
}


async function getSearchResults(f) {
    const response = await fetch('search/'+f.searchQuery);
    if (response.ok === true) {
        const data = await response.json();
        f.foundDocs = data;
	console.log(data);
    }
}
