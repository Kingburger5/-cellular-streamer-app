
/**
 * Securely downloads a file from a signed URL without redirecting the page.
 * @param url - The signed URL from your server
 * @param fileName - The desired filename for download
 */
export async function downloadFile(url: string, fileName: string) {
  try {
    // Fetch file as a blob
    const response = await fetch(url);
    if (!response.ok) throw new Error(`Failed to fetch file: ${response.statusText}`);
    const blob = await response.blob();

    // Create object URL for download
    const blobUrl = URL.createObjectURL(blob);
    const link = document.createElement("a");
    link.href = blobUrl;
    link.download = fileName; // forces browser to download
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);

    // Clean up object URL
    URL.revokeObjectURL(blobUrl);
  } catch (err) {
    console.error("Download failed:", err);
    throw err;
  }
}
