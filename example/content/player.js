document.addEventListener("DOMContentLoaded", () => {
    const entries = document.querySelectorAll('.audio-entry');
    entries.forEach(entry => {
        const src = entry.dataset.src;
        const id = src.replace(/[^\w\-\.]/g, '_');

        //create <audio>
        const audio = document.createElement('audio');
        audio.id = `audio-${id}`;
        audio.src = src;
        entry.appendChild(audio);

        //create toggle button (play/pause/continue)
        const toggleBtn = document.createElement('button');
        toggleBtn.id = `toggle-${id}`;
        toggleBtn.textContent = 'Play';
        toggleBtn.onclick = () => toggleAudio(id);
        entry.appendChild(toggleBtn);

        //create stop button
        const stopBtn = document.createElement('button');
        stopBtn.id = `stop-${id}`;
        stopBtn.textContent = 'Stop';
        stopBtn.style.display = 'none';
        stopBtn.onclick = () => stopAudio(id);
        entry.appendChild(stopBtn);
    });
});

let currentPlaying = null;

function toggleAudio(id) {
    const audio = document.getElementById(`audio-${id}`);
    const toggleBtn = document.getElementById(`toggle-${id}`);
    const stopBtn = document.getElementById(`stop-${id}`);

    if (currentPlaying && currentPlaying !== id) {
        stopAudio(currentPlaying);
    }

    if (audio.paused && audio.currentTime === 0) {
        audio.play();
        currentPlaying = id;
        toggleBtn.textContent = 'Pause';
        stopBtn.style.display = 'inline';
        hideOtherControls(id);
    }
    else if (!audio.paused) {
        audio.pause();
        toggleBtn.textContent = 'Continue';
    }
    else {
        audio.play();
        toggleBtn.textContent = 'Pause';
    }
}

function stopAudio(id) {
    const audio = document.getElementById(`audio-${id}`);
    const toggleBtn = document.getElementById(`toggle-${id}`);
    const stopBtn = document.getElementById(`stop-${id}`);

    audio.pause();
    audio.currentTime = 0;
    toggleBtn.textContent = 'Play';
    stopBtn.style.display = 'none';

    if (currentPlaying === id) currentPlaying = null;
    showAllControls();
}

function hideOtherControls(activeId) {
    const audios = document.querySelectorAll('audio');
    audios.forEach(audio => {
        const id = audio.id.replace('audio-', '');
        if (id !== activeId) {
            const toggleBtn = document.getElementById(`toggle-${id}`);
            const stopBtn = document.getElementById(`stop-${id}`);
            if (toggleBtn) toggleBtn.style.display = 'none';
            if (stopBtn) stopBtn.style.display = 'none';
        }
    });
}

function showAllControls() {
    const audios = document.querySelectorAll('audio');
    audios.forEach(audio => {
        const id = audio.id.replace('audio-', '');
        const toggleBtn = document.getElementById(`toggle-${id}`);
        if (toggleBtn) {
            toggleBtn.style.display = 'inline';
            toggleBtn.textContent = 'Play';
        }
    });
}
