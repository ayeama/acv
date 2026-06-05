#!/usr/bin/env python

from fastapi import FastAPI, WebSocket
from uvicorn import run


def main():
    app = FastAPI()

    @app.websocket("/ws")
    async def ws(websocket: WebSocket):
        await websocket.accept()
        while True:
            data = await websocket.receive_text()
            await websocket.send_text(data)

    run(app, host="localhost", port=8001)


if __name__ == "__main__":
    main()
