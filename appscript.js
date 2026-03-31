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
    .createTextOutput("Unknown POST action")
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
    content: props.getProperty("content") || "Atelier Vincent Kiefer",
    image_url: props.getProperty("image_url") || "",
    full_refresh: (props.getProperty("full_refresh") || "false") === "true",
    updated_at: new Date().toISOString()
  };

  // After delivering one payload with refresh request, clear full_refresh.
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
          .actions { margin-top: 16px; }
          .actions a { margin-right: 8px; }
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
          <div class="row"><span class="label">Bild URL:</span> ${escapeHtml(props.getProperty("image_url") || "")}</div>
        </div>
        <div class="actions">
          <a href="?action=admin">Zur Admin-Seite</a>
          <a href="?action=upload">Bild hochladen</a>
          <a href="?action=content">Content JSON</a>
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
  const props = getProps();
  const mode = props.getProperty("mode") || "text";

  const html = `
  <html>
  <head>
    <meta charset="utf-8">
    <title>Inkplate Admin</title>
    <style>
      body { font-family: Arial, sans-serif; margin: 40px; max-width: 700px; }
      input, textarea, select { width: 100%; margin-bottom: 10px; padding: 8px; }
      button, .button { padding: 10px 20px; font-size: 16px; margin-right: 8px; }
      .button { text-decoration: none; border: 1px solid #333; border-radius: 4px; display: inline-block; }
      .small { font-size: 14px; color: #555; margin-top: 10px; }
    </style>
  </head>
  <body>
    <h1>Inkplate Display Admin</h1>
    <form method="post">
      <input type="hidden" name="action" value="update">
      <label>Mode</label>
      <select name="mode">
        <option value="text" ${mode === "text" ? "selected" : ""}>text</option>
        <option value="image" ${mode === "image" ? "selected" : ""}>image</option>
      </select>

      <label>Text</label>
      <textarea name="content" rows="4">${escapeHtml(props.getProperty("content") || "")}</textarea>

      <label>Textgröße</label>
      <input name="size" value="${escapeHtml(props.getProperty("size") || "3")}">

      <label>Position X</label>
      <input name="pos_x" value="${escapeHtml(props.getProperty("pos_x") || "25")}">

      <label>Position Y</label>
      <input name="pos_y" value="${escapeHtml(props.getProperty("pos_y") || "60")}">

      <label>Bild URL (nur wenn Modus=image)</label>
      <input name="image_url" value="${escapeHtml(props.getProperty("image_url") || "")}">

      <button type="submit">Update Display</button>
      <a class="button" href="?action=upload">Bild hochladen</a>
      <a class="button" href="?action=status">Status</a>
    </form>

    <p class="small">Aktuelle Version: ${escapeHtml(props.getProperty("version") || "1")}</p>
    <p class="small">Aktueller Inhalt: ${escapeHtml(props.getProperty("content") || "(leer)")}</p>
  </body>
  </html>
  `;

  return HtmlService.createHtmlOutput(html);
}

function handleUpdate(e) {
  try {
    const props = PropertiesService.getScriptProperties();
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
        <head>
          <meta charset="utf-8">
          <title>Update Successful</title>
          <style>
            body { font-family: Arial, sans-serif; margin: 40px; }
          </style>
        </head>
        <body>
          <h2>Updated.</h2>
          <p>New version: ${version}</p>
          <a href="?action=admin">Back to Admin</a>
        </body>
      </html>
    `);
  } catch (error) {
    return HtmlService.createHtmlOutput(`
      <html>
        <head>
          <meta charset="utf-8">
          <title>Update Error</title>
          <style>
            body { font-family: Arial, sans-serif; margin: 40px; }
          </style>
        </head>
        <body>
          <h2>Error updating:</h2>
          <p>${error.message}</p>
          <a href="?action=admin">Back to Admin</a>
        </body>
      </html>
    `);
  }
}

function handleUploadPage() {
  return HtmlService.createHtmlOutput(`
  <html>
  <head>
    <meta charset="utf-8">
    <title>Bild Upload</title>
    <style>
      body { font-family: Arial, sans-serif; margin: 40px; max-width: 700px; }
      #status { margin-top: 16px; }
      .button { margin-top: 12px; display: inline-block; }
    </style>
  </head>
  <body>
    <h1>Bild Upload</h1>
    <input type="file" id="file" accept="image/*">
    <div id="status"></div>
    <p class="button">
      <a href="?action=admin">Zum Admin</a>
      <a href="?action=status">Status</a>
    </p>
    <script>
      const input = document.getElementById("file");
      const status = document.getElementById("status");

      input.onchange = async () => {
        const file = input.files[0];
        if (!file) {
          status.innerText = 'Keine Datei ausgewählt.';
          return;
        }

        status.innerText = 'Lade Datei hoch...';
        const reader = new FileReader();

        reader.onload = async () => {
          const data = reader.result.split(",")[1];

          try {
            const res = await fetch("", {
              method: "POST",
              headers: { "Content-Type": "application/json" },
              body: JSON.stringify({
                action: "upload",
                filename: file.name,
                data: data
              })
            });

            const json = await res.json();

            if (json.ok) {
              status.innerHTML =
                '<p>Upload erfolgreich.</p>' +
                '<p>Bild wird jetzt gesetzt und Mode auf "image" umgestellt.</p>' +
                '<p><a href="' + json.image_view_url + '" target="_blank">Bild anzeigen</a></p>' +
                '<p><a href="?action=admin">Zurück zu Admin</a></p>';
            } else {
              status.innerText = 'Upload fehlgeschlagen: ' + (json.error || 'Unbekannter Fehler');
            }
          } catch (err) {
            status.innerText = 'Upload fehlgeschlagen: ' + err;
          }
        };

        reader.readAsDataURL(file);
      };
    </script>
  </body>
  </html>
  `);
}

function handleUpload(body) {
  const props = getProps();
  if (!body || !body.data || !body.filename) {
    return ContentService
      .createTextOutput(JSON.stringify({ ok: false, error: 'Ungültiger Upload-Body' }))
      .setMimeType(ContentService.MimeType.JSON);
  }

  const blob = Utilities.newBlob(
    Utilities.base64Decode(body.data),
    'image/png',
    body.filename
  );

  const file = DriveApp.createFile(blob);

  const imageUrl = ScriptApp.getService().getUrl() + '?action=image&id=' + file.getId();

  props.setProperty('mode', 'image');
  props.setProperty('image_url', imageUrl);
  props.setProperty('content', '');
  props.setProperty('size', '3');
  props.setProperty('pos_x', '0');
  props.setProperty('pos_y', '0');
  props.setProperty('full_refresh', 'true');

  bumpVersion(props);

  return ContentService.createTextOutput(JSON.stringify({
    ok: true,
    image_url: imageUrl,
    image_view_url: imageUrl
  })).setMimeType(ContentService.MimeType.JSON);
}

function handleImage(e) {
  const id = e.parameter.id;
  if (!id) {
    return ContentService.createTextOutput('Missing image ID');
  }

  const file = DriveApp.getFileById(id);
  const blob = file.getBlob();

  return ContentService
    .createBinaryOutput(blob.getBytes())
    .setMimeType(blob.getContentType());
}
