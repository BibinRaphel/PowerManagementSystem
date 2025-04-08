from flask import Flask, request, jsonify, render_template_string

app = Flask(__name__)
data_log = []  # Store data in memory

HTML_PAGE = """
<!DOCTYPE html>
<html>
<head>
    <title>Energy Monitor Dashboard</title>
    <meta charset="UTF-8">
    <style>
        body { font-family: Arial; padding: 20px; background: #f0f0f0; }
        h1 { color: #333; }
        table { width: 100%%; border-collapse: collapse; }
        th, td { padding: 8px 12px; border: 1px solid #ccc; text-align: center; }
        th { background-color: #f9b234; color: white; }
    </style>
    <script>
        // Refresh the page every 5 seconds
        setTimeout(function(){
            window.location.reload();
        }, 5000);
    </script>
</head>
<body>
    <h1>Energy Monitor Dashboard</h1>
    <table>
        <tr>
            <th>Timestamp</th>
            <th>Appliance ID</th>
            <th>Power (W)</th>
            <th>Cumulative Energy (kWh)</th>
        </tr>
        {% for item in data %}
        <tr>
            <td>{{ item.timestamp }}</td>
            <td>{{ item.appliance_id }}</td>
            <td>{{ item.power_consumption }}</td>
            <td>{{ item.cumulative_energy }}</td>
        </tr>
        {% endfor %}
    </table>
</body>
</html>
"""

@app.route('/', methods=['GET'])
def dashboard():
    return render_template_string(HTML_PAGE, data=data_log)

@app.route('/upload', methods=['POST'])
def upload():
    content = request.get_json()
    if isinstance(content, list):
        data_log.extend(content)
        # Keep only the latest 15 entries
        if len(data_log) > 15:
            del data_log[:-15]
    return jsonify({"status": "received", "entries": len(content)})

if __name__ == '__main__':
    app.run(debug=True)
