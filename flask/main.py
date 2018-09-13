from flask import Flask, render_template, request, redirect, url_for

app = Flask(__name__)

# routing

@app.route('/')
def index():
    return render_template('index.html')

# main

if __name__ == '__main__':
    app.debug = True;
    app.run(host='0.0.0.0', port=5000) # run server