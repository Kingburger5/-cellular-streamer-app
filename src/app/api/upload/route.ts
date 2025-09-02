
import { NextRequest, NextResponse } from "next/server";
import { format } from 'date-fns';
import { getAdminStorage } from "@/lib/firebase-admin";

const BUCKET_NAME = "cellular-data-streamer.firebasestorage.app";

async function logRequestToSheet(request: NextRequest) {
    // This functionality has been temporarily disabled.
    console.log("Google Sheets logging is disabled.");
}


/**
 * Handles file uploads from transmission modules.
 * It saves the file to Firebase Storage inside the 'uploads' folder.
 */
export async function POST(request: NextRequest) {
    
    await logRequestToSheet(request);

    try {
        const contentTypeHeader = request.headers.get('content-type');
        if (!contentTypeHeader) {
             return NextResponse.json({ error: "Missing Content-Type header." }, { status: 400 });
        }
        
        const fileBuffer = await request.arrayBuffer();

        if (fileBuffer.byteLength === 0) {
             return NextResponse.json({ error: "Empty file content." }, { status: 400 });
        }
        
        const originalFilename = request.headers.get('x-original-filename') || `upload-${Date.now()}`;
        const filePath = `uploads/${originalFilename}`;
        
        const adminStorage = await getAdminStorage();
        const bucket = adminStorage.bucket(BUCKET_-NAME);
        const file = bucket.file(filePath);

        await file.save(Buffer.from(fileBuffer), {
            metadata: {
                contentType: contentTypeHeader,
            }
        });
        
        console.log(`[SERVER] Successfully uploaded to Firebase Storage: ${filePath}`);
        
        return NextResponse.json({
            message: "File uploaded successfully to Firebase Storage.",
            filename: originalFilename,
        }, { status: 200 });

    } catch (error: any) {
        console.error("[SERVER] Unhandled error in upload handler:", error);
        const errorMessage = error instanceof Error ? error.message : "An unknown error occurred on the server.";
        return NextResponse.json(
            { error: "Internal Server Error", details: errorMessage },
            { status: 500 }
        );
    }
}
