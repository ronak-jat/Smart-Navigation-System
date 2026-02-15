
"use client";

import { useEffect, useState, useRef } from "react";
import { useRouter } from "next/navigation";
import { auth, db } from "@/lib/firebase/config";
import { ref, onValue, set } from "firebase/database";
import { onAuthStateChanged } from "firebase/auth";
import { MapPin, Navigation, Lock, Unlock, Zap, Battery, Signal, LogOut } from "lucide-react";
import clsx from "clsx";

export default function DashboardPage() {
    const router = useRouter();
    const [user, setUser] = useState<any>(null);
    const [bikeId, setBikeId] = useState<string>("bike_001");
    const [bikeData, setBikeData] = useState<any>(null);
    const [destination, setDestination] = useState("");
    const mapRef = useRef<HTMLDivElement>(null);
    const googleMapRef = useRef<any>(null);
    const markerRef = useRef<any>(null);

    // 1. Auth Check
    useEffect(() => {
        const unsub = onAuthStateChanged(auth, (u) => {
            if (!u) router.push("/");
            else setUser(u);
        });
        return () => unsub();
    }, [router]);

    // 2. Firebase Realtime Listener
    useEffect(() => {
        if (!bikeId) return;
        const bikeRef = ref(db, `bikes/${bikeId}`);
        const unsub = onValue(bikeRef, (snapshot) => {
            const data = snapshot.val();
            setBikeData(data);

            // Update Map Marker
            if (data?.location && markerRef.current) {
                const title = `Bike: ${bikeId}`;
                markerRef.current.setPosition(data.location);
                markerRef.current.setTitle(title);
                if (!googleMapRef.current.getBounds().contains(markerRef.current.getPosition())) {
                    googleMapRef.current.panTo(data.location);

                }
            }
        });

        return () => unsub();
    }, [bikeId]);

    // 3. Load Google Maps
    useEffect(() => {
        // Check if script already loaded
        if ((window as any).google?.maps) {
            initMap();
            return;
        }

        const script = document.createElement("script");
        // TODO: Replace KEY with env var
        const apiKey = process.env.NEXT_PUBLIC_GOOGLE_MAPS_KEY || "";
        script.src = `https://maps.googleapis.com/maps/api/js?key=${apiKey}&libraries=places`;
        script.async = true;
        script.onload = () => initMap();
        document.body.appendChild(script);
    }, []);

    const initMap = () => {
        if (!mapRef.current) return;
        // Default: Jaipur
        const defaultPos = { lat: 27.176, lng: 75.956 };

        const map = new google.maps.Map(mapRef.current, {
            center: defaultPos,
            zoom: 15,
            disableDefaultUI: false,
            styles: [
                { elementType: "geometry", stylers: [{ color: "#242f3e" }] },
                { elementType: "labels.text.stroke", stylers: [{ color: "#242f3e" }] },
                { elementType: "labels.text.fill", stylers: [{ color: "#746855" }] },
                {
                    featureType: "administrative.locality",
                    elementType: "labels.text.fill",
                    stylers: [{ color: "#d59563" }],
                },
            ],
        });

        const marker = new google.maps.Marker({
            position: defaultPos,
            map: map,
            title: "Bike Location",
            icon: {
                path: google.maps.SymbolPath.CIRCLE,
                scale: 10,
                fillColor: "#3b82f6",
                fillOpacity: 1,
                strokeColor: "#ffffff",
                strokeWeight: 2,
            },
        });

        googleMapRef.current = map;
        markerRef.current = marker;
    };

    const handleNavigate = async () => {
        if (!bikeId || !destination) return;
        // Push command to Firebase
        await set(ref(db, `bikes/${bikeId}/command`), {
            type: "NAVIGATE",
            payload: destination,
            timestamp: Date.now()
        });
        alert(`Navigation sent to ${destination}`);
    };

    const toggleLock = async () => {
        const newStatus = bikeData?.isLocked ? "UNLOCK" : "LOCK";
        await set(ref(db, `bikes/${bikeId}/command`), {
            type: newStatus,
            timestamp: Date.now()
        });
    };

    return (
        <div className="flex flex-col h-screen bg-slate-950 text-white">
            {/* Header */}
            <header className="flex items-center justify-between px-6 py-4 bg-slate-900 border-b border-slate-800">
                <div className="flex items-center gap-3">
                    <div className="p-2 bg-blue-600 rounded-lg">
                        <Navigation className="w-5 h-5 text-white" />
                    </div>
                    <div>
                        <h1 className="font-bold text-lg leading-tight">Smart Bike</h1>
                        <p className="text-xs text-slate-400">ID: {bikeId}</p>
                    </div>
                </div>

                <div className="flex items-center gap-4">
                    <div className={clsx(
                        "flex items-center gap-2 px-3 py-1.5 rounded-full text-xs font-semibold border",
                        bikeData?.status === "online"
                            ? "bg-green-500/10 border-green-500/20 text-green-400"
                            : "bg-red-500/10 border-red-500/20 text-red-400"
                    )}>
                        <div className={clsx("w-2 h-2 rounded-full animate-pulse",
                            bikeData?.status === "online" ? "bg-green-400" : "bg-red-400"
                        )} />
                        {bikeData?.status === "online" ? "ONLINE" : "OFFLINE"}
                    </div>

                    <button onClick={() => auth.signOut()} className="p-2 hover:bg-slate-800 rounded-full transition-colors">
                        <LogOut className="w-5 h-5 text-slate-400" />
                    </button>
                </div>
            </header>

            {/* Main Content */}
            <main className="flex-1 relative">
                <div ref={mapRef} className="absolute inset-0 z-0 bg-slate-900" />

                {/* Overlay Controls */}
                <div className="absolute bottom-6 left-6 right-6 z-10 flex flex-col gap-4 max-w-md mx-auto">

                    {/* Status Card */}
                    <div className="bg-slate-900/90 backdrop-blur-md border border-slate-700 p-4 rounded-xl shadow-2xl flex items-center justify-between">
                        <div className="flex items-center gap-3">
                            <div className="p-2 bg-slate-800 rounded-lg">
                                <Battery className="w-5 h-5 text-green-400" />
                            </div>
                            <div>
                                <p className="text-xs text-slate-400">Battery</p>
                                <p className="font-bold">{bikeData?.battery || "--"}%</p>
                            </div>
                        </div>
                        <div className="w-px h-8 bg-slate-700" />
                        <div className="flex items-center gap-3">
                            <div className="p-2 bg-slate-800 rounded-lg">
                                <Signal className="w-5 h-5 text-blue-400" />
                            </div>
                            <div>
                                <p className="text-xs text-slate-400">Signal</p>
                                <p className="font-bold">LTE 4G</p>
                            </div>
                        </div>
                        <div className="w-px h-8 bg-slate-700" />
                        <div className="flex items-center gap-3">
                            <div className="p-2 bg-slate-800 rounded-lg">
                                <Zap className="w-5 h-5 text-yellow-400" />
                            </div>
                            <div>
                                <p className="text-xs text-slate-400">Range</p>
                                <p className="font-bold">~24 km</p>
                            </div>
                        </div>
                    </div>

                    {/* Navigation Card */}
                    <div className="bg-slate-900/90 backdrop-blur-md border border-slate-700 p-4 rounded-xl shadow-2xl space-y-4">
                        <div className="relative">
                            <MapPin className="absolute left-3 top-1/2 -translate-y-1/2 w-5 h-5 text-slate-400" />
                            <input
                                type="text"
                                placeholder="Where to?"
                                value={destination}
                                onChange={(e) => setDestination(e.target.value)}
                                className="w-full bg-slate-800 border border-slate-600 rounded-lg py-3 pl-10 pr-4 text-white placeholder-slate-500 focus:ring-2 focus:ring-blue-500 outline-none"
                            />
                        </div>

                        <div className="grid grid-cols-4 gap-3">
                            <button
                                onClick={handleNavigate}
                                className="col-span-3 bg-blue-600 hover:bg-blue-700 text-white font-bold py-3 rounded-lg transition-colors flex items-center justify-center gap-2"
                            >
                                <Navigation className="w-5 h-5" />
                                Start Navigation
                            </button>

                            <button
                                onClick={toggleLock}
                                className={clsx(
                                    "col-span-1 border font-bold py-3 rounded-lg transition-colors flex items-center justify-center",
                                    bikeData?.isLocked
                                        ? "bg-red-500/10 border-red-500/50 text-red-500 hover:bg-red-500/20"
                                        : "bg-green-500/10 border-green-500/50 text-green-500 hover:bg-green-500/20"
                                )}
                            >
                                {bikeData?.isLocked ? <Lock className="w-5 h-5" /> : <Unlock className="w-5 h-5" />}
                            </button>
                        </div>
                    </div>

                </div>
            </main>
        </div>
    );
}
