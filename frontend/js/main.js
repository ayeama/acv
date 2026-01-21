document.addEventListener("alpine:init", () => {
    Alpine.data("acv", () => ({
        async info() {
            const response = await fetch("/api/v1/system/info");
            if (response.status == 200) {
                return await response.json();
            }
        },
    }));
});
