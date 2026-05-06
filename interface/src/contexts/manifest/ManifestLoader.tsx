import { FC, useCallback, useEffect, useMemo, useState } from 'react';

import * as ManifestApi from '../../api/manifest';
import { configureFsEndpoints } from '../../api/fs';
import { RequiredChildrenProps, extractErrorMessage } from '../../utils';
import { FeatureEntry, UiManifest } from '../../types';

import { ManifestContext, ManifestContextValue } from './context';
import ManifestProgress from './ManifestProgress';

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

// Loader keeps the rest of the tree mounted in the background — the
// ManifestProgress overlay sits on top until the manifest has been
// fetched AND its module list has finished its staggered reveal,
// then unmounts. Children always have access to the (initially empty)
// manifest context, so any module that subscribes survives an
// inflight fetch without an extra null-check.
const ManifestLoader: FC<RequiredChildrenProps> = (props) => {
  const [manifest, setManifest] = useState<UiManifest>(EMPTY_MANIFEST);
  const [loaded, setLoaded] = useState<boolean>(false);
  const [error, setError] = useState<string | undefined>();
  const [revealComplete, setRevealComplete] = useState<boolean>(false);

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

  return (
    <ManifestContext.Provider value={value}>
      {props.children}
      {!revealComplete && (
        <ManifestProgress
          loaded={loaded}
          manifest={manifest}
          error={error}
          onRevealComplete={() => setRevealComplete(true)}
        />
      )}
    </ManifestContext.Provider>
  );
};

export default ManifestLoader;
