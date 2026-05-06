import { FC, useCallback, useEffect, useMemo, useState } from 'react';

import * as ManifestApi from '../../api/manifest';
import { configureFsEndpoints } from '../../api/fs';
import { RequiredChildrenProps, extractErrorMessage } from '../../utils';
import { FeatureEntry, UiManifest } from '../../types';

import { ManifestContext, ManifestContextValue } from './context';

const EMPTY_MANIFEST: UiManifest = {
  schemaVersion: 0,
  features: []
};

// Map feature entries to the module-level path tables that non-hook code
// (api/fs.ts, …) reads from. Called whenever the manifest changes.
const applyManifest = (manifest: UiManifest): void => {
  const fs = manifest.features.find((f) => f.id === 'filesystem');
  configureFsEndpoints(fs?.endpoints);
};

// Non-blocking loader: children always render. If the manifest fetch fails or
// is delayed, consumers get an empty features[] and should fall back to their
// hardcoded legacy behavior.
const ManifestLoader: FC<RequiredChildrenProps> = (props) => {
  const [manifest, setManifest] = useState<UiManifest>(EMPTY_MANIFEST);
  const [loaded, setLoaded] = useState<boolean>(false);
  const [error, setError] = useState<string | undefined>();

  const loadManifest = useCallback(async () => {
    try {
      const response = await ManifestApi.readManifest();
      const data = response.data;
      const next = data && Array.isArray(data.features) ? data : EMPTY_MANIFEST;
      setManifest(next);
      applyManifest(next);
      setLoaded(true);
      setError(undefined);
    } catch (err: any) {
      setError(extractErrorMessage(err, 'Failed to fetch UI manifest.'));
      setManifest(EMPTY_MANIFEST);
      setLoaded(true);
    }
  }, []);

  useEffect(() => {
    loadManifest();
  }, [loadManifest]);

  const findFeature = useCallback(
    (id: string): FeatureEntry | undefined => manifest.features.find((f) => f.id === id),
    [manifest]
  );

  const value: ManifestContextValue = useMemo(
    () => ({ manifest, loaded, error, findFeature }),
    [manifest, loaded, error, findFeature]
  );

  return <ManifestContext.Provider value={value}>{props.children}</ManifestContext.Provider>;
};

export default ManifestLoader;
