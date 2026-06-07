/* Worst script known to man */
/* Sources:
   https://www.w3schools.com/howto/howto_js_lightbox.asp
   https://css-tricks.com/prevent-page-scrolling-when-a-modal-is-open/
*/

function openModal() {
  document.body.style.position = 'fixed';
  document.body.style.top = `-${window.scrollY}px`;
  document.getElementById("myModal").style.display = "block";
  document.getElementById("myModal").focus();
  setImageIndex(0);
}

function closeModal() {
    document.getElementById("myModal").style.display = "none";
    const scrollY = document.body.style.top;
    document.body.style.position = '';
    document.body.style.top = '';
    window.scrollTo(0, parseInt(scrollY || '0') * -1);
}

function isModalOpen() {
    return (document.getElementById("myModal").style.display == "block");
}

function formatLines(str) {
    let lines = str.split("\n")
    lines = lines.filter(line => !line.startsWith("Difference in frames"))
    return lines.join("<br>")
}

function extractItem(elem) {
    return {
        name: elem.querySelector("h1").innerText,
        beforeImg: elem.querySelector(".before").getAttribute("src"),
        afterImg: elem.querySelector(".after").getAttribute("src"),
        details: formatLines(elem.querySelector("pre").innerText)
    };
}

const items = [...document.querySelectorAll(".item")].map(extractItem)
let currentImage = 0;
let currentState = 0;

function getImageIndexForUri(uri) {
    for (let i = 0; i < items.length; i++) {
        if (items[i].beforeImg == uri || items[i].afterImg == uri)
            return i;
    }
    return -1;
}

function setImageState(state) {
    const item = items[currentImage]
    const uri = (state === 0) ? item.beforeImg : item.afterImg;
    const stateText = (state === 0) ? "BEFORE" : "AFTER";
    const posText = "(" + (currentImage + 1).toString() + "/" + (items.length).toString() + ") ";
    document.getElementById("compareImage").setAttribute("src", uri);
    document.getElementById("compareState").innerText = stateText;
    document.getElementById("compareTitle").innerText = posText + item.name;
    document.getElementById("compareCaption").innerHTML = item.details;
    currentState = state;
}

function setImageIndex(index) {
    if (index < 0 || index > items.length)
        return;

    currentImage = index;
    setImageState(0);
}

function handleKey(key) {
    if (key == " ") {
        setImageState((currentState === 0) ? 1 : 0);
        return true;
    } else if (key == "ArrowLeft") {
        setImageIndex(currentImage - 1);
        return true;
    } else if (key == "ArrowRight") {
        setImageIndex(currentImage + 1);
        return true;
    } else if (key == "Escape") {
        closeModal();
        return true;
    } else {
        console.log(key);
        return false;
    }
}

document.getElementById("myModal").addEventListener("keydown", function(ev) {
    if (ev.defaultPrevented)
        return;
    
    if (handleKey(ev.key))
        ev.preventDefault();
});

document.querySelector("#myModal .prev").addEventListener("click", function() {
    setImageIndex(currentImage - 1);
});
document.querySelector("#myModal .next").addEventListener("click", function() {
    setImageIndex(currentImage + 1);
});
document.querySelectorAll(".item img").forEach(elem => elem.addEventListener("click", function() {
    if (!isModalOpen())
        openModal();
    setImageIndex(getImageIndexForUri(this.getAttribute("src")));
}));