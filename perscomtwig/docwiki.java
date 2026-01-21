<script>
let currentDocKey = null;

function loadDoc(url, title, copyUrl, docKey) {
  currentDocKey = docKey;

  document.getElementById('viewer').src = url;
  document.getElementById('docTitle').textContent = title;

  // Open button
  const openBtn = document.getElementById('openBtn');
  openBtn.href = url;
  openBtn.style.display = 'inline-block';

  // Copy button
  const copyBtn = document.getElementById('copyBtn');
  if (copyUrl && copyUrl.length) {
    copyBtn.href = copyUrl;
    copyBtn.style.display = 'inline-block';
  } else {
    copyBtn.style.display = 'none';
  }
}

/* Copy sharable link */
document.getElementById('copyLinkBtn').addEventListener('click', function () {
  if (!currentDocKey) return;

  const url = new URL(window.location.href);
  url.searchParams.set('doc', currentDocKey);

  navigator.clipboard.writeText(url.toString())
    .then(() => alert('Link copied to clipboard'))
    .catch(() => alert('Failed to copy link'));
});

/* Bookmark (update URL without reload) */
document.getElementById('bookmarkBtn').addEventListener('click', function () {
  if (!currentDocKey) return;

  const url = new URL(window.location.href);
  url.searchParams.set('doc', currentDocKey);

  history.replaceState({}, '', url.toString());
  alert('Document bookmarked');
});

/* Auto-load bookmarked document on page load */
(function () {
  const params = new URLSearchParams(window.location.search);
  const key = params.get('doc');
  if (!key) return;

  const links = document.querySelectorAll('#docsMenu a[onclick]');
  for (const link of links) {
    if (link.getAttribute('onclick').includes("'" + key + "'")) {
      link.click();
      break;
    }
  }
})();
</script>
