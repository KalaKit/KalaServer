document.addEventListener("DOMContentLoaded", () => {
    const entries = document.querySelectorAll(".video-entry");

    entries.forEach(entry => {
        const src = entry.dataset.src;
        const label = entry.textContent.trim();

        //create a styled button link
        const button = document.createElement("a");
        button.href = src;
        button.textContent = label;
        button.style.display = "inline-block";
        button.style.margin = "10px 0";
        button.style.padding = "10px 20px";
        button.style.backgroundColor = "#28a745";
        button.style.color = "#fff";
        button.style.textDecoration = "none";
        button.style.borderRadius = "5px";
        button.style.fontFamily = "sans-serif";

        //replace original entry with the styled button
        entry.replaceWith(button);
    });
});