import { NextRequest, NextResponse } from "next/server";
import { getStorage } from 'firebase-admin/storage';
import { adminApp } from "@/lib/firebase-admin";

async function getUploadUrl(filePath: string): Promise<string> {
    const bucket = getStorage(adminApp).bucket();
    const file = bucket.file(filePath);
    const expires = Date.now() + 60 * 60 * 1000; // 1 hour

    const [url] = await file.getSignedUrl({
        action: 'write',
        expires: expires,
        contentType: 'application/octet-stream',
    });

    return url;
}


export async function POST(request: NextRequest) {
  try {
    const filename = request.headers.get("x-filename");
    if (!filename) {
      return NextResponse.json({ error: "x-filename header is required." }, { status: 400 });
    }

    const signedUrl = await getUploadUrl(filename);
    
    const readable = request.body;
    if (!readable) {
        return NextResponse.json({ error: "Request body is empty" }, { status: 400 });
    }
    
    const response = await fetch(signedUrl, {
        method: 'PUT',
        headers: {
            'Content-Type': 'application/octet-stream'
        },
        body: readable,
        cache: 'no-store',
        // @ts-ignore
        duplex: 'half'
    });

    if (response.ok) {
        console.log(`File uploaded successfully: ${filename}`);
        return NextResponse.json({ message: "File uploaded successfully.", filename: filename }, { status: 200 });
    } else {
        const errorText = await response.text();
        console.error("Upload to GCS failed:", response.status, errorText);
        return NextResponse.json({ error: "Failed to upload to Google Cloud Storage", details: errorText }, { status: response.status });
    }
    
  } catch (error) {
    console.error("Upload error:", error);
    const errorMessage = error instanceof Error ? error.message : "Internal Server Error";
    return NextResponse.json(
      { error: "Internal Server Error", details: errorMessage },
      { status: 500 }
    );
  }
}
