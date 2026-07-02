/*
 * This script acts as a simple, secure bridge to the
 * Google Translate API.
 */
function doGet(e) {
  // Get parameters from the URL
  var text = e.parameter.text;
  var sourceLang = e.parameter.source; // e.g., "en"
  var targetLang = e.parameter.target; // e.g., "es"

  // Check if all parameters are present
  if (!text || !sourceLang || !targetLang) {
    return ContentService.createTextOutput("Error: Missing parameters. Need 'text', 'source', and 'target'.");
  }

  try {
    // This is the line that does the translation
    var translatedText = LanguageApp.translate(text, sourceLang, targetLang);

    // Send back *only the plain text*
    return ContentService.createTextOutput(translatedText);

  } catch(err) {
    return ContentService.createTextOutput("Error: " + err.message);
  }
}