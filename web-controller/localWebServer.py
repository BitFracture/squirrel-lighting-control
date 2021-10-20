from flask import Flask, render_template, send_from_directory, redirect, url_for, request, session
import sys
import json

class LocalWebServer:
    app = Flask(__name__)
    app.config['TEMPLATES_AUTO_RELOAD'] = True
    app.config['IP'] = "0.0.0.0"
    app.config['PORT'] = "8081"
    
    command = None
    clientList = None
    lock = None

    @app.route("/", methods=['GET'])
    def route_index():
        app = LocalWebServer.app
        clients = list()
        for client in LocalWebServer.clientList:
            clients.append(json.loads(client))
        return render_template('index.html', clientList=clients)
        
    @app.route("/command", methods=['POST'])
    def route_cmd():
        app = LocalWebServer.app
        clients = list()
        body = request.get_json(force=True)
        command = body.get("command", "").strip();
        print("Command set to: {}".format(command), file=sys.stderr)
        LocalWebServer.command["value"] = command + "\n"
        return "Good"

    def start_server(command, clientList, lock):
        app = LocalWebServer.app
        LocalWebServer.command = command
        LocalWebServer.clientList = clientList
        LocalWebServer.lock = lock
        app.run(host=app.config["IP"], port=app.config["PORT"])
