function doGet(e) {
  const action = (e.parameter.action || "").trim();

  switch (action) {
    case "content":
      return handleContent();

    case "report":
      return handleReport(e);

    case "status":
      return handleStatus();

    case "admin":
      return handleAdmin();

    case "upload":
      return handleUploadPage();

    default:
      return handleStatus();
  }
}

function doPost(e) {
  try {

    // JSON upload
    if (e.postData && e.postData.contents) {
      const body = JSON.parse(e.postData.contents);

      if (body.action === "upload") {
        return handleUpload(body);
      }
    }

  } catch (err) {}

  // FORM POST fallback
  if (e.parameter) {
    return handleUpdate(e);
  }

  return ContentService
    .createTextOutput("Unknown POST request")
    .setMimeType(ContentService.MimeType.TEXT);
}

function getProps() {
  return PropertiesService.getScriptProperties();
}

function bumpVersion(props) {
  let version = parseInt(props.getProperty("version") || "1", 10);
  version++;
  props.setProperty("version", version.toString());
  props.setProperty("full_refresh", "true");
  return version;
}

function handleContent() {
  const props = getProps();

  const payload = {
    version: parseInt(props.getProperty("version") || "1", 10),
    mode: props.getProperty("mode") || "text",
    size: parseInt(props.getProperty("size") || "3", 10),
    pos_x: parseInt(props.getProperty("pos_x") || "25", 10),
    pos_y: parseInt(props.getProperty("pos_y") || "60", 10),
    content: props.getProperty("content") || "",
    image_url: props.getProperty("image_url") || "",
    full_refresh: (props.getProperty("full_refresh") || "false") === "true",
    updated_at: new Date().toISOString()
  };

  if (payload.full_refresh) {
    props.setProperty("full_refresh", "false");
  }

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
    .createTextOutput(JSON.stringify({ ok: true, received_at: now }))
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
  body{font-family:Arial;margin:24px}
  .card{border:1px solid #ccc;border-radius:8px;padding:16px;max-width:650px}
  .row{margin:8px 0}
  .label{font-weight:bold;width:200px;display:inline-block}
  </style>
  </head>
  <body>

  <h1>Inkplate Status</h1>

  <div class="card">

  <div class="row"><span class="label">Soll-Version:</span>${props.getProperty("version")||""}</div>
  <div class="row"><span class="label">Modus:</span>${props.getProperty("mode")||""}</div>
  <div class="row"><span class="label">Gerät:</span>${props.getProperty("last_device")||""}</div>
  <div class="row"><span class="label">Event:</span>${props.getProperty("last_event")||""}</div>
  <div class="row"><span class="label">Version bestätigt:</span>${props.getProperty("last_version")||""}</div>
  <div class="row"><span class="label">Zuletzt gesehen:</span>${props.getProperty("last_seen_at")||""}</div>
  <div class="row"><span class="label">RSSI:</span>${props.getProperty("last_rssi")||""}</div>
  <div class="row"><span class="label">Message:</span>${props.getProperty("last_message")||""}</div>

  </div>

  <br>

  <a href="?action=admin">Admin</a> |
  <a href="?action=upload">Upload</a> |
  <a href="?action=content">Content JSON</a>

  </body>
  </html>
  `;

  return HtmlService.createHtmlOutput(html);
}

function handleAdmin() {
  const props = getProps();
  const mode = props.getProperty("mode") || "text";

  const html = `
  <html>
  <head>
  <meta charset="utf-8">
  <title>Inkplate Admin</title>
  <style>
  body{font-family:Arial;margin:40px;max-width:650px}
  input,textarea,select{width:100%;padding:8px;margin-bottom:10px}
  button{padding:10px 20px}
  </style>
  </head>

  <body>

  <h1>Inkplate Admin</h1>

  <form method="post" action="?action=update">

  <label>Mode</label>
  <select name="mode">
  <option value="text" ${mode==="text"?"selected":""}>text</option>
  <option value="image" ${mode==="image"?"selected":""}>image</option>
  </select>

  <label>Text</label>
  <textarea name="content" rows="4">${props.getProperty("content")||""}</textarea>

  <label>Text Size</label>
  <input name="size" value="${props.getProperty("size")||"3"}">

  <label>X</label>
  <input name="pos_x" value="${props.getProperty("pos_x")||"25"}">

  <label>Y</label>
  <input name="pos_y" value="${props.getProperty("pos_y")||"60"}">

  <label>Image URL</label>
  <input name="image_url" value="${props.getProperty("image_url")||""}">

  <button type="submit">Update Display</button>

  </form>

  <br>

  <a href="?action=upload">Upload Image</a> |
  <a href="?action=status">Status</a>

  </body>
  </html>
  `;

  return HtmlService.createHtmlOutput(html);
}

function handleUpdate(e) {
  const props = getProps();
  const p = e.parameter;

  props.setProperty("mode", p.mode || "text");
  props.setProperty("content", p.content || "");
  props.setProperty("size", p.size || "3");
  props.setProperty("pos_x", p.pos_x || "25");
  props.setProperty("pos_y", p.pos_y || "60");
  props.setProperty("image_url", p.image_url || "");

  const version = bumpVersion(props);

  return HtmlService.createHtmlOutput(`
  <html>
  <body style="font-family:Arial;margin:40px">

  <h2>Updated</h2>
  <p>New version: ${version}</p>

  <a href="?action=admin">Back</a>

  </body>
  </html>
  `);
}

function handleUploadPage() {
  return HtmlService.createHtmlOutput(`
  <html>
  <body style="font-family:Arial;margin:40px">

  <h1>Upload Image</h1>

  <input type="file" id="file">

  <div id="status"></div>

  <script>

  const input = document.getElementById("file")
  const status = document.getElementById("status")

  input.onchange = async () => {

  const file = input.files[0]

  const reader = new FileReader()

  reader.onload = async () => {

  const data = reader.result.split(",")[1]

  const res = await fetch("",{
  method:"POST",
  headers:{"Content-Type":"application/json"},
  body:JSON.stringify({
  action:"upload",
  filename:file.name,
  data:data
  })
  })

  const json = await res.json()

  status.innerHTML = "Uploaded<br><a href='?action=admin'>Back</a>"

  }

  reader.readAsDataURL(file)

  }

  </script>

  </body>
  </html>
  `);
}

function handleUpload(body) {
  const props = getProps();

  const blob = Utilities.newBlob(
    Utilities.base64Decode(body.data),
    "image/png",
    body.filename
  );

  const file = DriveApp.createFile(blob);

  file.setSharing(DriveApp.Access.ANYONE_WITH_LINK, DriveApp.Permission.VIEW);

  const imageUrl = "https://drive.google.com/uc?export=view&id=" + file.getId();

  props.setProperty("mode", "image");
  props.setProperty("image_url", imageUrl);
  props.setProperty("content", "");
  props.setProperty("full_refresh", "true");

  bumpVersion(props);

  return ContentService
    .createTextOutput(JSON.stringify({ ok: true, image_url: imageUrl }))
    .setMimeType(ContentService.MimeType.JSON);
}