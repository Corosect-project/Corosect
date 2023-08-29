# Requirements

- A linux pc with bluetooth and BlueZ installed
- Python 3.10 at least

# Installation

Start by creating python virtual environment in this folder with

```
python3 -m venv .venv
```

The environment has to be called `.venv`.

Next activate the environment `source .venv/bin/activate` and install dependencies

```
python3 -m pip install -r min-requirements.txt
```

or

```
python3 -m pip install bluepy==1.3.0
python3 -m pip install paho-mqtt==1.6.1
```

Make sure that both `observer.py` and `gatt_con.py` are
marked as executable.

# Usage

To scan for devices run either the
`observer.py` or `gatt_con.py` with

```
sudo ./observer.py
sudo ./gatt_con.py
```

To stop the program either stop it with `ctrl + c` or
if there is local mqtt server that uses default port send
message to `CONTROL` topic with message string `QUIT` to
quit gracefully.

The device the script looks for is defined by the device's
complete local name and it can be changed in `config.py` by
changing the `device_name` variable.
