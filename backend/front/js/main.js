CONFIG_KEY = "acv.config"

const inputHandle = document.getElementById('inputHandle');
const inputRpmMin = document.getElementById('inputRpmMin');
const inputRpmMax = document.getElementById('inputRpmMax');
const inputRpmIdle = document.getElementById('inputRpmIdle');
const inputRpmPowerband = document.getElementById('inputRpmPowerband');
const inputRpmRedline = document.getElementById('inputRpmRedline');
const inputThrottlePositionMin = document.getElementById('inputThrottlePositionMin');
const inputThrottlePositionMax = document.getElementById('inputThrottlePositionMax');

class Config {
    constructor() {
        this.theme = 'dark';
        this.handle = '@zx4rr_gal';
        this.rpm_min = 0;
        this.rpm_max = 18000;
        this.rpm_idle = 2500;
        this.rpm_powerband = 11500;
        this.rpm_redline = 16000;
        this.throttle_position_min = 12;
        this.throttle_position_max = 44;

        this.load()
        this.populate()
    }
    
    load() {
        const json = localStorage.getItem(CONFIG_KEY)
        
        if (!json) {
            return;
        }
        
        Object.assign(this, JSON.parse(json))
    }

    save() {
        localStorage.setItem(CONFIG_KEY, JSON.stringify(this));
    }
    
    populate() {
        inputHandle.value = this.handle;
        inputRpmMin.value = this.rpm_min;
        inputRpmMax.value = this.rpm_max;
        inputRpmIdle.value = this.rpm_idle;
        inputRpmPowerband.value = this.rpm_powerband;
        inputRpmRedline.value = this.rpm_redline;
        inputThrottlePositionMin.value = this.throttle_position_min;
        inputThrottlePositionMax.value = this.throttle_position_max;

        document.getElementById('handle').textContent = inputHandle.value;
    }

    update() {
        this.handle = inputHandle.value
        this.rpm_min = Number(inputRpmMin.value)
        this.rpm_max = Number(inputRpmMax.value)
        this.rpm_idle = Number(inputRpmIdle.value)
        this.rpm_powerband = Number(inputRpmPowerband.value)
        this.rpm_redline = Number(inputRpmRedline.value)
        this.throttle_position_min = Number(inputThrottlePositionMin.value)
        this.throttle_position_max = Number(inputThrottlePositionMax.value)
    }
}

const config = new Config();

const alertSave = document.getElementById("alertSave");
const buttonSave = document.getElementById('buttonSave').addEventListener('click', () => {
    config.update();
    config.save();
    config.populate();

    alertSave.classList.remove("d-none");
    setTimeout(() => {
        alertSave.classList.add("d-none");
    }, 2000);
})

const chat = {
    coolant_temp: 0,
    rpm: 0,
    speed: 0,
    intake_air_temp: 0,
    throttle_position: 0,
}

class LiveStream {
    constructor() {
        this.url = 'ws://acv.local/ws';
        this.socket = null;
        this.onmessage = null;
        this.reconnect = true;
        this.reconnect_timeout = 1000;
        this.connected = false;

        this.connect();
    }

    connect() {
        this.socket = new WebSocket(this.url);

        this.socket.onopen = () => {
            console.log('livestream connected');
            this.connected = true;
        };

        this.socket.onclose = () => {
            console.log('livestream disconnected');

            this.connected = false;
            this.socket = null;

            if (this.reconnect) {
                setTimeout(() => {
                    this.connect();
                }, this.reconnect_timeout);
            }
        };

        this.socket.onerror = (error) => {
            console.error('livestream error', error);
        };

        this.socket.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                if (this.onmessage) {
                    this.onmessage(data);
                }
            } catch (error) {
                console.error('livestream message error', error)
            }
        };
    }

    disconnect() {
        this.reconnect = false;

        if (this.socket) {
            this.socket.close();
            this.socket = null;
        }
    }
}

const livestream = new LiveStream();
livestream.onmessage = (data) => {
    // TODO: try this
    // Object.assign(chat, data)

    if (typeof data.rpm === 'number') {
        chat.rpm = data.rpm;
    }
    if (typeof data.speed === 'number') {
        chat.speed = data.speed;
    }
    if (typeof data.coolant_temp === 'number') {
        chat.coolant_temp = data.coolant_temp;
    }
    if (typeof data.intake_air_temp === 'number') {
        chat.intake_air_temp = data.intake_air_temp;
    }
    if (typeof data.throttle_position === 'number') {
        chat.throttle_position = data.throttle_position;
    }
}

// TODO: use cookies to store feature flags? rpm smoothing, bike profiles etc.

function preloadImages() {
    let assets = ['/images/offline.webp', '/images/idle.webp', '/images/redline.webp'];

    for (let i = 1; i <= 16; i++) {
        assets.push(`/images/${i}.webp`);
    }

    for (let i of assets) {
        const img = new Image();
        img.src = i;
    }
}

preloadImages();

const img = document.getElementById('animation');

const rpmText = document.getElementById('rpmText');
const rpmProgress = document.getElementById('rpmProgress')

const speedText = document.getElementById('speedText');
const speedProgress = document.getElementById('speedProgress');

const coolantTempText = document.getElementById('coolantTempText');
const coolantTempProgress = document.getElementById('coolantTempProgress');

const intakeAirTempText = document.getElementById('intakeAirTempText');
const intakeAirTempProgress = document.getElementById('intakeAirTempProgress');

const throttlePositionText = document.getElementById('throttlePositionText');
const throttlePositionProgress = document.getElementById('throttlePositionProgress');


let lastSrc = null;
function setImage(src) {
    if (lastSrc === src) return;
    lastSrc = src;
    img.src = src;
}

function percentage(min, max, value) {
    return ((value - min) / (max - min)) * 100;
}

function render_rpm() {
    const rpm_min = config.rpm_min;
    const rpm_max = config.rpm_max;

    rpmText.textContent = chat.rpm.toLocaleString('en-AU', {
        style: 'currency',
        currency: 'AUD',
        minimumFractionDigits: 0,
        maximumFractionDigits: 0,
    });
    rpmProgress.setAttribute('aria-valuenow', `${chat.rpm}`);
    rpmProgress.children[0].style.width = `${percentage(rpm_min, rpm_max, chat.rpm)}%`;
}

function render_speed() {
    const speed_min = 0;
    const speed_max = 255;

    speedText.textContent = `${chat.speed}`;
    speedProgress.setAttribute('aria-valuenow', `${chat.speed}`);
    speedProgress.children[0].style.width = `${percentage(speed_min, speed_max, chat.speed)}%`;
}

function render_coolant_temp() {
    const coolant_temp_min = -40;
    const coolant_temp_max = 215;

    coolantTempText.textContent = `${chat.coolant_temp}`;
    coolantTempProgress.setAttribute('aria-valuenow', `${chat.coolant_temp}`);
    coolantTempProgress.children[0].style.width = `${percentage(coolant_temp_min, coolant_temp_max, chat.coolant_temp)}%`;
}

function render_intake_air_temp() {
    const intake_air_temp_min = -40;
    const intake_air_temp_max = 215;

    intakeAirTempText.textContent = `${chat.intake_air_temp}`;
    intakeAirTempProgress.setAttribute('aria-valuenow', `${chat.intake_air_temp}`);
    intakeAirTempProgress.children[0].style.width = `${percentage(intake_air_temp_min, intake_air_temp_max, chat.intake_air_temp)}%`;
}

function render_throttle_position() {
    const throttle_position_min = config.throttle_position_min;
    const throttle_position_max = config.throttle_position_max;

    throttlePositionText.textContent = `${Math.round(chat.throttle_position)}`;
    throttlePositionProgress.setAttribute('aria-valuenow', `${chat.throttle_position}`);
    throttlePositionProgress.children[0].style.width = `${percentage(throttle_position_min, throttle_position_max, chat.throttle_position)}%`;
}

function render_animation() {
    if (!livestream.connected) {
        setImage('/images/offline.webp');
        return;
    }
    if (chat.rpm <= config.rpm_idle) {
        setImage('/images/idle.webp');
        return;
    }
    if (chat.rpm >= config.rpm_powerband) {
        setImage('/images/redline.webp');
        return;
    }

    const frames = 16;
    let normalized = (chat.rpm - config.rpm_idle) / (config.rpm_powerband - config.rpm_idle);
    normalized = Math.max(0, Math.min(1, normalized));

    const frameIndex = Math.floor(normalized * (frames - 1)) + 1;

    setImage(`/images/${frameIndex}.webp`);
}

function render() {
    render_rpm();
    render_speed();
    render_coolant_temp();
    render_intake_air_temp();
    render_throttle_position();

    render_animation();
}

function debug_sweep(k, x, min, max, step) {
    let value = min;
    let dir = 1;

    const intervalKey = `__sweep_${k}`;

    if (window[intervalKey]) {
        clearInterval(window[intervalKey]);
    }

    window[intervalKey] = setInterval(() => {
        value += step * dir;
        if (value >= max) {
            value = max;
            dir = -1;
        }
        if (value <= min) {
            value = min;
            dir = 1;
        }
        x(Math.round(value));
    }, 10);
}

function debug(sweep) {
    chat.rpm = 2500;
    chat.speed = 60;
    chat.coolant_temp = 37;
    chat.intake_air_temp = 29;
    chat.throttle_position = 10.0;

    if (sweep) {
        debug_sweep(
            'rpm',
            (x) => chat.rpm = x,
            config.rpm_idle,
            config.rpm_redline,
            48
        );
        debug_sweep(
            'speed',
            (x) => chat.speed = x,
            0,
            255,
            0.6
        );
        debug_sweep(
            'coolant_temp',
            (x) => chat.coolant_temp = x,
            -40,
            215,
            0.4
        );
        debug_sweep(
            'intake_air_temp',
            (x) => chat.intake_air_temp = x,
            -40,
            215,
            0.5
        );
        debug_sweep(
            'throttle_position',
            (x) => chat.throttle_position = x,
            0,
            100,
            0.3
        );
    } else {
        for (const k in window) {
            if (k.startsWith('__sweep_')) {
                clearInterval(window[k]);
                delete window[k];
            }
        }
    }
}

function loop() {
    render();
    requestAnimationFrame(loop);
}

requestAnimationFrame(loop);
