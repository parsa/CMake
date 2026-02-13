Redo
----

.. versionadded:: 4.2

Generates ``.do`` scripts into the build tree for building with `redo`.

Usage
^^^^^

Specify the generator when invoking :manual:`cmake(1)`:

.. code-block:: shell

  cmake [<options>] -G Redo -B <path-to-build> [-S <path-to-source>]

Build with :manual:`cmake(1)`:

.. code-block:: shell

  cmake --build <path-to-build> [--target <tgt>]

Builtin Targets
^^^^^^^^^^^^^^^

``all``

  Depends on all targets required by the project, except those with
  :prop_tgt:`EXCLUDE_FROM_ALL` set to true.

``install``

  Runs the install step.

``clean``

  Removes generated build outputs.

Notes
^^^^^

* This generator requires the ``redo`` and ``redo-ifchange`` programs.
  The ``redo`` program is discovered as :variable:`CMAKE_MAKE_PROGRAM`.
* This generator is single-config only.
* Custom commands support ``OUTPUT`` and ``BYPRODUCTS`` (including multiple
  outputs). Outputs are staged and then materialized per-output to preserve
  redo's single-target semantics.
* Custom commands that declare directory outputs are supported. Nested outputs
  under a directory output are treated as part of the directory output.
* Job pools and terminal serialization are supported as **scheduling
  constraints**:

  - ``JOB_POOL`` on custom commands is mapped to a redo pool with the same
    name, with depth taken from :prop_gbl:`JOB_POOLS` (or :variable:`CMAKE_JOB_POOLS`).
  - ``USES_TERMINAL`` on custom commands is mapped to a ``console`` pool with
    depth ``1``.
  - :prop_sf:`JOB_POOL_COMPILE` (source) overrides :prop_tgt:`JOB_POOL_COMPILE`
    (target) for object compilation.
  - :prop_tgt:`JOB_POOL_LINK` is used for linking.

  The redo build engine enforces these pools using its jobserver scheduler.
  Console behavior is best-effort and may differ from Ninja's interactive TTY
  handling.

See Also
^^^^^^^^

* :manual:`cmake-generators(7)`

