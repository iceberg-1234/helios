def decide_action(mode: str, lux_left: float, lux_center: float, lux_right: float, dist_cm: float, threshold: float = 50.0):
    """
    Core steering logic matching helios.ino.
    Returns: 'AVOID_OBSTACLE', 'TURN_LEFT', 'TURN_RIGHT', 'FORWARD', or 'STOP'
    """
    if dist_cm < 20.0:
        return 'AVOID_OBSTACLE'

    if mode == 'STOP':
        return 'STOP'

    if mode == 'SUN':
        if lux_left > lux_center + threshold and lux_left > lux_right:
            return 'TURN_LEFT'
        elif lux_right > lux_center + threshold and lux_right > lux_left:
            return 'TURN_RIGHT'
        else:
            return 'FORWARD'

    if mode == 'SHADE':
        if lux_left < lux_center - threshold and lux_left < lux_right:
            return 'TURN_LEFT'
        elif lux_right < lux_center - threshold and lux_right < lux_left:
            return 'TURN_RIGHT'
        else:
            return 'FORWARD'

    return 'STOP'


def test_decide_action():
    # Obstacle check
    assert decide_action('SUN', 1000, 1000, 1000, dist_cm=15.0) == 'AVOID_OBSTACLE'

    # Sun mode: left brightest
    assert decide_action('SUN', 500, 200, 200, dist_cm=50.0) == 'TURN_LEFT'
    # Sun mode: right brightest
    assert decide_action('SUN', 200, 200, 600, dist_cm=50.0) == 'TURN_RIGHT'
    # Sun mode: balanced / center brightest
    assert decide_action('SUN', 400, 500, 420, dist_cm=50.0) == 'FORWARD'

    # Shade mode: left darkest
    assert decide_action('SHADE', 100, 400, 500, dist_cm=50.0) == 'TURN_LEFT'
    # Shade mode: right darkest
    assert decide_action('SHADE', 500, 400, 100, dist_cm=50.0) == 'TURN_RIGHT'
    # Shade mode: balanced
    assert decide_action('SHADE', 400, 390, 410, dist_cm=50.0) == 'FORWARD'

    # Stop mode
    assert decide_action('STOP', 500, 500, 500, dist_cm=50.0) == 'STOP'

    print("All Helios logic self-checks passed!")


if __name__ == '__main__':
    test_decide_action()
