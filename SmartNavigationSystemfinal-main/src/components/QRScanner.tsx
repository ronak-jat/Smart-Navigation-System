
"use client";

import { useEffect, useRef, useState } from "react";
import { Html5QrcodeScanner } from "html5-qrcode";

interface QRScannerProps {
    onScan: (data: string) => void;
    onError?: (err: any) => void;
}

export default function QRScanner({ onScan, onError }: QRScannerProps) {
    const scannerRef = useRef<Html5QrcodeScanner | null>(null);
    const [scanResult, setScanResult] = useState<string | null>(null);

    useEffect(() => {
        // Prevent double initialization in React Strict Mode
        if (scannerRef.current) return;

        const scanner = new Html5QrcodeScanner(
            "reader",
            { fps: 10, qrbox: { width: 250, height: 250 } },
      /* verbose= */ false
        );
        scannerRef.current = scanner;

        scanner.render(
            (data) => {
                setScanResult(data);
                onScan(data);
                scanner.clear(); // Stop scanning on success
            },
            (err) => {
                if (onError) onError(err);
            }
        );

        return () => {
            if (scannerRef.current) {
                scannerRef.current.clear().catch(console.error);
            }
        };
    }, [onScan, onError]);

    return (
        <div className="w-full max-w-sm mx-auto">
            <div id="reader" className="overflow-hidden rounded-lg border border-slate-700 bg-black"></div>
            {scanResult && (
                <div className="mt-4 p-4 bg-green-500/10 border border-green-500/20 text-green-400 rounded-lg text-center">
                    Success! Scanned: {scanResult}
                </div>
            )}
        </div>
    );
}
