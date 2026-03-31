function doGet(e) {
  const action = (e.parameter.action || "").trim();

  if (action === "content") {
    return handleContent();
  }

  if (action === "report") {
    return handleReport(e);
  }

  if (action === "status") {
    return handleStatus();
  }

  if (action === "admin") {
    return handleAdmin();
  }

  if (action === "upload") {
  return handleUploadPage();
}

if (action === "image") {
  return handleImage(e);
}

  return ContentService
    .createTextOutput("Missing or invalid action")
    .setMimeType(ContentService.MimeType.TEXT);
}

function doPost(e) {

  let action = "";

  // JSON request
  if (e.postData && e.postData.contents) {
    try {
      const body = JSON.parse(e.postData.contents);
      action = body.action || "";
      if (action === "upload") {
        return handleUpload(body);
      }
    } catch (err) {
      // not JSON
    }
  }

  // form request
  action = (e.parameter.action || "").trim();

  if (action === "update") {
    return handleUpdate(e);
  }

  if (action === "report") {
    return handleReport(e);
  }

  return ContentService
    .createTextOutput("Unknown POST action");
}

function getProps() {
  return PropertiesService.getScriptProperties();
}

function handleContent() {
  const props = getProps();

  const payload = {
    version: parseInt(props.getProperty("version") || "1", 10),
    mode: props.getProperty("mode") || "text",
    size: parseInt(props.getProperty("size") || "3", 10),
    pos_x: parseInt(props.getProperty("pos_x") || "25", 10),
    pos_y: parseInt(props.getProperty("pos_y") || "60", 10),
    content: props.getProperty("content") || "Atelier Vincent Kiefer",
    image_url: props.getProperty("image_url") || "",
    full_refresh: (props.getProperty("full_refresh") || "false") === "true",
    updated_at: new Date().toISOString()
  };

  return ContentService
    .createTextOutput(JSON.stringify(payload))
    .setMimeType(ContentService.MimeType.JSON);
}

function handleReport(e) {
  const props = getProps();
  const p = e.parameter || {};

  const now = new Date().toISOString();

  props.setProperty("last_device", p.device || "unknown");
  props.setProperty("last_event", p.event || "unknown");
  props.setProperty("last_version", p.version || "");
  props.setProperty("last_seen_at", now);
  props.setProperty("last_rssi", p.rssi || "");
  props.setProperty("last_message", p.message || "");

  return ContentService
    .createTextOutput(JSON.stringify({
      ok: true,
      received_at: now
    }))
    .setMimeType(ContentService.MimeType.JSON);
}

function handleStatus() {
  const props = getProps();

  const html = `
    <html>
      <head>
        <meta charset="utf-8">
        <title>Inkplate Status</title>
        <style>
          body { font-family: Arial, sans-serif; margin: 24px; line-height: 1.5; }
          h1 { margin-bottom: 16px; }
          .card { border: 1px solid #ccc; border-radius: 10px; padding: 16px; max-width: 700px; }
          .row { margin: 8px 0; }
          .label { font-weight: bold; display: inline-block; width: 220px; }
        </style>
      </head>
      <body>
        <h1>Inkplate Status</h1>
        <div class="card">
          <div class="row"><span class="label">Aktuelle Soll-Version:</span> ${escapeHtml(props.getProperty("version") || "")}</div>
          <div class="row"><span class="label">Modus:</span> ${escapeHtml(props.getProperty("mode") || "")}</div>
          <div class="row"><span class="label">Letztes Gerät:</span> ${escapeHtml(props.getProperty("last_device") || "")}</div>
          <div class="row"><span class="label">Letztes Event:</span> ${escapeHtml(props.getProperty("last_event") || "")}</div>
          <div class="row"><span class="label">Zuletzt bestätigte Version:</span> ${escapeHtml(props.getProperty("last_version") || "")}</div>
          <div class="row"><span class="label">Letzte Rückmeldung:</span> ${escapeHtml(props.getProperty("last_seen_at") || "")}</div>
          <div class="row"><span class="label">RSSI:</span> ${escapeHtml(props.getProperty("last_rssi") || "")}</div>
          <div class="row"><span class="label">Nachricht:</span> ${escapeHtml(props.getProperty("last_message") || "")}</div>
        </div>
      </body>
    </html>
  `;

  return HtmlService.createHtmlOutput(html);
}

function escapeHtml(str) {
  return String(str)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

function handleAdmin() {

  const props = PropertiesService.getScriptProperties();

  const html = `
  <html>
  <head>
  <meta charset="utf-8">
  <title>Inkplate Admin</title>

  <style>

  body{
    font-family:Arial;
    margin:40px;
    max-width:700px;
  }

  input, textarea, select{
    width:100%;
    margin-bottom:10px;
    padding:8px;
  }

  button{
    padding:10px 20px;
    font-size:16px;
  }

  </style>

  </head>

  <body>

  <h1>Inkplate Display Admin</h1>

  <form method="post">

  <input type="hidden" name="action" value="update">

  <label>Mode</label>
  <select name="mode">
    <option value="text">text</option>
    <option value="image">image</option>
  </select>

  <label>Text</label>
  <textarea name="content" rows="4">${props.getProperty("content") || ""}</textarea>

  <label>Textgröße</label>
  <input name="size" value="${props.getProperty("size") || 3}">

  <label>Position X</label>
  <input name="pos_x" value="${props.getProperty("pos_x") || 25}">

  <label>Position Y</label>
  <input name="pos_y" value="${props.getProperty("pos_y") || 60}">

  <label>Bild URL</label>
  <input name="image_url" value="${props.getProperty("image_url") || ""}">

  <button type="submit">Update Display</button>

  </form>

  <p>Current version: ${props.getProperty("version")}</p>

  </body>
  </html>
  `;

  return HtmlService.createHtmlOutput(html);
}

function handleUpdate(e) {

  const props = PropertiesService.getScriptProperties();
  const p = e.parameter;

  props.setProperty("mode", p.mode || "text");
  props.setProperty("content", p.content || "");
  props.setProperty("size", p.size || "3");
  props.setProperty("pos_x", p.pos_x || "25");
  props.setProperty("pos_y", p.pos_y || "60");
  props.setProperty("image_url", p.image_url || "");

  let version = parseInt(props.getProperty("version") || "1");
  version++;

  props.setProperty("version", version.toString());

  return HtmlService.createHtmlOutput(
    "<h2>Updated.</h2><p>New version: " + version + "</p><a href='?action=admin'>Back</a>"
  );
}

function handleUploadPage() {

return HtmlService.createHtmlOutput(`

<h2>Bild Upload</h2>

<input type="file" id="file">

<script>

const input = document.getElementById("file");

input.onchange = async () => {

  const file = input.files[0];
  const reader = new FileReader();

  reader.onload = async () => {

    const data = reader.result.split(",")[1];

    const res = await fetch("", {
      method:"POST",
      body:JSON.stringify({
        action:"upload",
        filename:file.name,
        data:data
      })
    });

    const text = await res.text();
    document.body.innerHTML += "<p>"+text+"</p>";

  };

  reader.readAsDataURL(file);

};

</script>

`);
}

function handleUpload(body) {

  const blob = Utilities.newBlob(
    Utilities.base64Decode(body.data),
    "image/png",
    body.filename
  );

  const file = DriveApp.createFile(blob);

  return ContentService.createTextOutput(
    ScriptApp.getService().getUrl() +
    "?action=image&id=" + file.getId()
  );
}

function handleImage(e){

const id = e.parameter.id;

const file = DriveApp.getFileById(id);
const blob = file.getBlob();

return ContentService
.createBinaryOutput(blob.getBytes())
.setMimeType(blob.getContentType());

}