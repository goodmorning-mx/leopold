CONTROL_COMPONENT_IDS = frozenset({
    "computer-volume-down",
    "computer-volume-mute",
    "computer-volume-up",
    "computer-screenshot",
})


def computer_control_screen() -> dict:
    return {
        "id": "computer-controls",
        "background": "#000000",
        "components": [
            {
                "type": "text", "id": "computer-title", "text": "COMPUTER",
                "x": 233, "y": 80, "align": "center", "size": 3,
                "color": "#FFFFFF",
            },
            {
                "type": "text", "id": "computer-status", "text": "MAC CONTROLS",
                "x": 233, "y": 126, "align": "center", "size": 1,
                "color": "#A7A7A7",
            },
            {
                "type": "button", "id": "computer-volume-down", "label": "VOL -",
                "x": 64, "y": 180, "width": 96, "height": 72,
                "color": "#FFFFFF",
            },
            {
                "type": "button", "id": "computer-volume-mute", "label": "MUTE",
                "x": 185, "y": 180, "width": 96, "height": 72,
                "color": "#FD9D20",
            },
            {
                "type": "button", "id": "computer-volume-up", "label": "VOL +",
                "x": 306, "y": 180, "width": 96, "height": 72,
                "color": "#FFFFFF",
            },
            {
                "type": "button", "id": "computer-screenshot", "label": "CAPTURE",
                "x": 113, "y": 292, "width": 240, "height": 74,
                "color": "#3D7FFF",
            },
        ],
    }


def with_computer_controls(document: dict) -> dict:
    screens = list(document.get("screens") or [])
    if not any(screen.get("id") == "computer-controls" for screen in screens):
        screens.append(computer_control_screen())
    return {**document, "screens": screens}
