
import { NextRequest, NextResponse } from "next/server";
import { format } from 'date-fns';
import { google } from 'googleapis';
import { adminStorage } from "@/lib/firebase-admin";

async function logRequestToSheet(request: NextRequest) {
    const SPREADSHEET_ID = process.env.GOOGLE_SHEET_ID;
    const LOG_SHEET_NAME = "ConnectionLog";
    const GOOGLE_SERVICE_ACCOUNT_CREDENTIALS = process.env.GOOGLE_SERVICE_ACCOUNT_CREDENTIALS;
     
    if (!SPREADSHEET_ID || !GOOGLE_SERVICE_ACCOUNT_CREDENTIALS) {
      console.log("Google Sheets env vars for logging not set. Skipping.");
      return;
    }

    try {
        const credentials = JSON.parse(GOOGLE_SERVICE_ACCOUNT_CREDENTIALS);
        const auth = new google.auth.GoogleAuth({
            credentials,
            scopes: ['https://www.googleapis.com/auth/spreadsheets'],
        });
        const sheets = google.sheets({ version: 'v4', auth });

        const timestamp = format(new Date(), "yyyy-MM-dd'T'HH:mm:ss.SSSxxx");
        const ip = request.ip ?? request.headers.get('x-forwarded-for') ?? 'N/A';
        const userAgent = request.headers.get('user-agent') || 'N/A';
        const contentType = request.headers.get('content-type') || 'N-A';
        
        const headers: string[] = [];
        request.headers.forEach((value, key) => {
            if (key.toLowerCase().startsWith('x-')) {
                headers.push(`${key}: ${value}`);
            }
        });

        const newRow = [
            timestamp,
            ip,
            userAgent,
            contentType,
            headers.join('\n')
        ];
        
        await sheets.spreadsheets.values.append({
            spreadsheetId: SPREADSHEET_ID,
            range: `${LOG_SHEET_NAME}!A1`,
            valueInputOption: 'USER_ENTERED',
            requestBody: {
                values: [newRow],
            },
        });

    } catch (error) {
        console.error("Failed to write request log to Google Sheet:", error);
    }
}


/**
 * Handles file uploads from transmission modules.
 * It saves the file to Firebase Storage.
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
        
        const filename = request.headers.get('x-original-filename') || `upload-${Date.now()}`;
        
        const bucketName = process.env.NEXT_PUBLIC_FIREBASE_STORAGE_BUCKET;
        if (!bucketName) {
            throw new Error("Firebase Storage bucket name is not configured in environment variables.");
        }
        const bucket = adminStorage.bucket(bucketName);
        const file = bucket.file(`uploads/${filename}`);

        await file.save(Buffer.from(fileBuffer), {
            metadata: {
                contentType: contentTypeHeader,
            }
        });
        
        console.log(`[SERVER] Successfully uploaded to Firebase Storage: ${filename}`);
        
        return NextResponse.json({
            message: "File uploaded successfully to Firebase Storage.",
            filename: filename,
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
