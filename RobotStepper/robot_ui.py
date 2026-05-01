#!/usr/bin/env python3
import threading, json, os
import rclpy
from rclpy.node import Node
from std_msgs.msg import String
from http.server import HTTPServer, BaseHTTPRequestHandler

class RobotNode(Node):
    def __init__(self):
        super().__init__('robot_ui_node')
        self.pubs = {
            1: self.create_publisher(String, '/robot1/cmd', 10),
            2: self.create_publisher(String, '/robot2/cmd', 10),
        }
    def send(self, robot_id, cmd):
        msg = String(); msg.data = cmd
        self.pubs[robot_id].publish(msg)
        self.get_logger().info(f'robot{robot_id} >> {cmd}')
    def send_all(self, cmd):
        for pub in self.pubs.values():
            msg = String(); msg.data = cmd
            pub.publish(msg)
        self.get_logger().info(f'ALL >> {cmd}')

robot_node = None

class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_GET(self):
        self.send_response(200)
        self.send_header('Content-Type','text/html')
        self.end_headers()
        path = os.path.join(os.path.dirname(__file__), 'robot_ui.html')
        with open(path, 'rb') as f:
            self.wfile.write(f.read())
    def do_POST(self):
        length = int(self.headers.get('Content-Length', 0))
        body   = json.loads(self.rfile.read(length))
        robot  = body.get('robot', 0)
        cmd    = body['cmd']
        if robot == 0:
            robot_node.send_all(cmd)
        else:
            robot_node.send(robot, cmd)
        self.send_response(200)
        self.end_headers()
        self.wfile.write(b'ok')

def run():
    global robot_node
    rclpy.init()
    robot_node = RobotNode()
    t = threading.Thread(target=rclpy.spin, args=(robot_node,), daemon=True)
    t.start()
    print('Robot UI at http://localhost:8080')
    HTTPServer(('0.0.0.0', 8080), Handler).serve_forever()

if __name__ == '__main__':
    run()
