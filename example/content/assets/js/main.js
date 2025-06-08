document.addEventListener("DOMContentLoaded", () => {
    console.log("Admin panel loaded successfully.");

    const footer = document.querySelector(".admin-footer");

    function adjustFooterPosition() {
        const bodyHeight = document.body.scrollHeight;
        const windowHeight = window.innerHeight;

        if (bodyHeight <= windowHeight) {
            footer.classList.add("fixed");
        } else {
            footer.classList.remove("fixed");
        }
    }

    adjustFooterPosition();
    window.addEventListener("resize", adjustFooterPosition);
});