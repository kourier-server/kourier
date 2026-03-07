function searchPost(value)
{
    const searchBarCancelBtn = document.getElementById('searchBarCancelBtn');
    if (!value)
    {
        searchBarCancelBtn.style.display = 'none';
    }
    else
    {
        searchBarCancelBtn.style.display = 'block';
    }
}


function clearSearchText()
{
    const searchBar = document.getElementById('searchBarTextId');
    const searchBarCancelBtn = document.getElementById('searchBarCancelBtn');
    searchBar.value = "";
    searchBarCancelBtn.style.display = 'none';
}
