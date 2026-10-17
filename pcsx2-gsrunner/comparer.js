/* Worst script known to man */
/* Sources:
   https://www.w3schools.com/howto/howto_js_lightbox.asp
   https://css-tricks.com/prevent-page-scrolling-when-a-modal-is-open/
*/

function openModal(index, frameIndex) {
  // Freeze the page in place without changing its layout: keep the body at full
  // width and keep the scrollbar track, otherwise every image on the page gets
  // re-laid out, rescaled and re-decoded (they blank out for a few seconds).
  document.documentElement.style.overflowY = 'scroll';
  document.body.style.width = '100%';
  document.body.style.position = 'fixed';
  document.body.style.top = `-${window.scrollY}px`;
  document.getElementById("myModal").style.display = "block";
  document.getElementById("myModal").focus();
  // Fall back to the first image if no (or an invalid) position was given
  setImageIndex(index >= 0 ? index : 0, frameIndex >= 0 ? frameIndex : 0);
}

function closeModal() {
    document.getElementById("myModal").style.display = "none";
    const scrollY = document.body.style.top;
    document.body.style.position = '';
    document.body.style.top = '';
    document.body.style.width = '';
    document.documentElement.style.overflowY = '';
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

function getFrameNumber(uri) {
    const m = /_frame([0-9]+)\.png/.exec(uri || "");
    return m ? m[1] : "?";
}

function extractItem(elem) {
    const befores = [...elem.querySelectorAll(".before")];
    const afters = [...elem.querySelectorAll(".after")];
    const frames = befores.map((before, i) => {
        const beforeImg = before.getAttribute("src");
        const afterImg = afters[i] ? afters[i].getAttribute("src") : beforeImg;
        return {
            number: getFrameNumber(beforeImg),
            beforeImg: beforeImg,
            afterImg: afterImg
        };
    });
    // Make sure frames are in numerical order (frame00001, frame00002, ...)
    frames.sort((a, b) => parseInt(a.number, 10) - parseInt(b.number, 10));

    return {
        name: elem.querySelector("h1").innerText,
        frames: frames,
        details: formatLines(elem.querySelector("pre").innerText)
    };
}

const items = [...document.querySelectorAll(".item")].map(extractItem)
let currentImage = 0;
let currentFrame = 0;
let currentState = 0;

/* Returns [itemIndex, frameIndex], or [-1, -1] if not found */
function getImageIndexForUri(uri) {
    for (let i = 0; i < items.length; i++) {
        const frames = items[i].frames;
        for (let f = 0; f < frames.length; f++) {
            if (frames[f].beforeImg == uri || frames[f].afterImg == uri)
                return [i, f];
        }
    }
    return [-1, -1];
}

function setImageState(state) {
    const item = items[currentImage]
    const frame = item.frames[currentFrame];
    const uri = (state === 0) ? frame.beforeImg : frame.afterImg;
    const stateText = (state === 0) ? "BEFORE" : "AFTER";
    const posText = "(" + (currentImage + 1).toString() + "/" + (items.length).toString() + ") ";
    const frameText = "Frame " + parseInt(frame.number, 10).toString() + " [" + (currentFrame + 1).toString() + "/" + (item.frames.length).toString() + "]";
    document.getElementById("compareImage").setAttribute("src", uri);
    const stateElem = document.getElementById("compareState");
    stateElem.innerText = stateText;
    // Frame info goes on its own right-aligned line beneath the image
    let frameElem = document.getElementById("compareFrame");
    if (!frameElem) {
        frameElem = document.createElement("div");
        frameElem.id = "compareFrame";
        frameElem.style.textAlign = "right";
        stateElem.parentNode.insertBefore(frameElem, stateElem.nextSibling);
    }
    frameElem.innerText = frameText;
    // Match the look of the title at the top (colour, size, font) without needing CSS changes
    const titleStyle = window.getComputedStyle(document.getElementById("compareTitle"));
    frameElem.style.color = titleStyle.color;
    frameElem.style.fontSize = titleStyle.fontSize;
    frameElem.style.fontFamily = titleStyle.fontFamily;
    frameElem.style.fontWeight = titleStyle.fontWeight;
    document.getElementById("compareTitle").innerText = posText + item.name;
    document.getElementById("compareCaption").innerHTML = item.details;
    currentState = state;
}

function setImageIndex(index, frameIndex) {
    if (index < 0 || index >= items.length)
        return;

    currentImage = index;
    currentFrame = frameIndex || 0;
    setImageState(0);
}

/* Move to the next/previous frame of the current item, clamped at both ends.
   Keeps the current BEFORE/AFTER state so frames are easy to compare. */
function setFrameIndex(frameIndex) {
    const frames = items[currentImage].frames;
    if (frameIndex < 0 || frameIndex >= frames.length)
        return;

    currentFrame = frameIndex;
    setImageState(currentState);
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
    } else if (key == "ArrowDown") {
        setFrameIndex(currentFrame + 1);
        return true;
    } else if (key == "ArrowUp") {
        setFrameIndex(currentFrame - 1);
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
    const [i, f] = getImageIndexForUri(this.getAttribute("src"));
    if (!isModalOpen())
        openModal(i, f);
    else
        setImageIndex(i, f);
}));