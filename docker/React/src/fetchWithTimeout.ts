export default function (url: RequestInfo, options?: RequestInit, timeout = 1000): Promise<Response> {
    const controller = new AbortController();
    setTimeout(() => controller.abort(), timeout);
    if (!options) {
        options = {};
    }
    options.signal = controller.signal;
    return fetch(url, options);
}